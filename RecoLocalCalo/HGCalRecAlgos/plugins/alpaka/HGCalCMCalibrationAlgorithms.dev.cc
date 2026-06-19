#include <cstddef>
#include <cstdint>
#include <algorithm>  // for std::clamp

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "HeterogeneousCore/AlpakaInterface/interface/traits.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

#include "RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCMCalibrationAlgorithms.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  // ---------------------------------------------------------------------------
  // Kernel: fill CM calibration ML input SoA, one thread per digi.
  //
  // Indexing: digis are stored in global channel order, so digi_view[idx],
  // mlsoa[idx], and index_view[idx] all correspond to global channel idx.
  // The kernel iterates ndigis (the actual per-event digi count), not maxDataSize().
  //
  // DNN input order (21 floats, must match training):
  //   cm0..cm11 | msubchidx | msuberxidx | cellfrac | unconn0..3 | ntoa | ntot
  //
  // Correction is subtractive: corrected_adc = raw_adc - model_prediction.
  // ---------------------------------------------------------------------------
  struct HGCalCMCalibKernel_fillInputs {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  HGCalSoACMMLDeviceCollection::View mlsoa,
                                  hgcaldigi::HGCalDigiDevice::ConstView digi_view,
                                  hgcal::HGCalDenseIndexInfoDevice::ConstView index_view,
                                  hgcal::HGCalMappingCellParamDevice::ConstView cellmap_view,
                                  uint32_t const* __restrict__ d_chDataOffsets,
                                  uint32_t const* __restrict__ d_enabledErx,
                                  uint32_t ndigis,
                                  float ntoa,
                                  float ntot) const {
      for (auto idx : uniform_elements(acc, ndigis)) {
        auto const idxinfo = index_view[idx];
        uint32_t const denseModIdx = idxinfo.modInfoIdx();
        uint32_t const chIdx = idxinfo.chNumber();  // 0..(nErx*37-1) within module
        // enabledErxMask is a bitmask: bit e set means eRx e is present.
        uint32_t const enabledErxMask = d_enabledErx[denseModIdx];
        // Count of enabled eRxs for mean-subtraction: __builtin_popcount works on device.
        uint32_t const nErx = uint32_t(__builtin_popcount(enabledErxMask));
        uint32_t const chOffset = d_chDataOffsets[denseModIdx];  // digi SoA offset for this module
        uint32_t const erxIdx = chIdx / 37u;

        auto slot = mlsoa[idx];

        // Mean-subtracted channel index (unique per channel, referenced to module mean).
        slot.msubchidx() = float(chIdx) - (float(nErx) * 37.0f - 1.0f) / 2.0f;

        // Mean-subtracted eRx index (same for all channels in the same eRx).
        slot.msuberxidx() = float(erxIdx) - (float(nErx) - 1.0f) / 2.0f;

        // Cell area fraction (SF key in the cell-parameter mapping).
        slot.cellfrac() = cellmap_view[idxinfo.cellInfoIdx()].trace();

        // Event-level scalars, identical for every channel in the event.
        slot.ntoa() = ntoa;
        slot.ntot() = ntot;

        // CM sum per eRx (cm0..cm11): digi.cm() is the pedestal-subtracted
        // sum of the 2 CM channels for eRx e.  Read from the first channel of each eRx
        // (chOffset + e*37).  Guard with the bitmask so inactive eRx slots stay 0.
#define FILL_CM(e_) \
        slot.cm##e_() = ((enabledErxMask >> uint32_t(e_)) & 1u) \
                        ? float(digi_view[chOffset + uint32_t(e_) * 37u].cm()) : 0.0f
        FILL_CM(0);  FILL_CM(1);  FILL_CM(2);  FILL_CM(3);
        FILL_CM(4);  FILL_CM(5);  FILL_CM(6);  FILL_CM(7);
        FILL_CM(8);  FILL_CM(9);  FILL_CM(10); FILL_CM(11);
#undef FILL_CM

        // 4 individual unconnected channel ADCs in this digi's eRx.
        // Within-eRx positions: 8, 17, 19, 28 (same for all LD modules).
        constexpr uint32_t kUnconn[4] = {8u, 17u, 19u, 28u};
        slot.unconn0() = float(digi_view[chOffset + erxIdx * 37u + kUnconn[0]].adc());
        slot.unconn1() = float(digi_view[chOffset + erxIdx * 37u + kUnconn[1]].adc());
        slot.unconn2() = float(digi_view[chOffset + erxIdx * 37u + kUnconn[2]].adc());
        slot.unconn3() = float(digi_view[chOffset + erxIdx * 37u + kUnconn[3]].adc());
      }
    }
  };

  // ---------------------------------------------------------------------------
  // Kernel: apply subtractive DNN correction to digi ADC in-place.
  // corrected_adc = raw_adc - model_prediction  (model predicts the noise)
  // ---------------------------------------------------------------------------
  struct HGCalCMCalibKernel_applyCorrections {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  hgcaldigi::HGCalDigiDevice::View digi_view,
                                  HGCalCMCorrectionDeviceCollection::ConstView corr_view,
                                  uint32_t ndigis) const {
      for (auto idx : uniform_elements(acc, ndigis)) {
        float corrected = float(digi_view[idx].adc()) - corr_view[idx].correction();
        digi_view[idx].adc() = uint16_t(std::clamp(corrected, 0.0f, 65535.0f));
      }
    }
  };

  // ---------------------------------------------------------------------------
  // fillCMInputs: fill ML input SoA from digis, one slot per digi.
  // ---------------------------------------------------------------------------
  void HGCalCMCalibrationAlgorithms::fillCMInputs(Queue& queue,
                                                   uint32_t ndigis,
                                                   int ntoa,
                                                   int ntot,
                                                   hgcaldigi::HGCalDigiDevice const& device_digis,
                                                   hgcal::HGCalDenseIndexInfoDevice const& device_index,
                                                   hgcal::HGCalMappingCellParamDevice const& device_cellmap,
                                                   uint32_t const* d_chDataOffsets,
                                                   uint32_t const* d_enabledErx,
                                                   HGCalSoACMMLDeviceCollection& device_mlsoa) const {
    LogDebug("HGCalCMCalibrationAlgorithms") << "fillCMInputs: ndigis=" << ndigis
                                              << " ntoa=" << ntoa << " ntot=" << ntot;

    uint32_t items = uint32_t(n_threads_);
    uint32_t groups = divide_up_by(ndigis, items);
    auto grid = make_workdiv<Acc1D>(groups, items);

    alpaka::exec<Acc1D>(queue,
                        grid,
                        HGCalCMCalibKernel_fillInputs{},
                        device_mlsoa.view(),
                        device_digis.const_view(),
                        device_index.const_view(),
                        device_cellmap.const_view(),
                        d_chDataOffsets,
                        d_enabledErx,
                        ndigis,
                        float(ntoa),
                        float(ntot));
  }

  // ---------------------------------------------------------------------------
  // applyCMCorrections: subtract DNN prediction from digi ADC in-place.
  // ---------------------------------------------------------------------------
  void HGCalCMCalibrationAlgorithms::applyCMCorrections(
      Queue& queue,
      uint32_t ndigis,
      HGCalCMCorrectionDeviceCollection const& device_corrections,
      hgcaldigi::HGCalDigiDevice& device_digis) const {
    LogDebug("HGCalCMCalibrationAlgorithms") << "applyCMCorrections: ndigis=" << ndigis;

    uint32_t items = uint32_t(n_threads_);
    uint32_t groups = divide_up_by(ndigis, items);
    auto grid = make_workdiv<Acc1D>(groups, items);

    alpaka::exec<Acc1D>(queue,
                        grid,
                        HGCalCMCalibKernel_applyCorrections{},
                        device_digis.view(),
                        device_corrections.const_view(),
                        ndigis);
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE
