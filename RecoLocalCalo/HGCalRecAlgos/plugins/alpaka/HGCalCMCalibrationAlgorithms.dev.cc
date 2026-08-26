#include <cstddef>
#include <cstdint>
#include <algorithm>  // for std::clamp
#include <cmath>      // for std::round

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
  // DNN input order (21 floats, must match training -- see HGCALSoACMML.h):
  //   cm0..cm11 | ntoa | ntot | msubchidx | msuberxidx | cellfrac | unconn0..3
  //
  // Correction is subtractive: corrected_adc = raw_adc - model_prediction.
  // ---------------------------------------------------------------------------
  // CM feature scale: 2 = full scale (cm - 2*CM_ped), the convention the DNN was trained on.
  static constexpr float kCMScale = 2.0f;

  struct HGCalCMCalibKernel_fillInputs {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  HGCalSoACMMLDeviceCollection::View mlsoa,
                                  hgcaldigi::HGCalDigiDevice::ConstView digi_view,
                                  hgcal::HGCalDenseIndexInfoDevice::ConstView index_view,
                                  hgcal::HGCalMappingCellParamDevice::ConstView cellmap_view,
                                  uint32_t const* __restrict__ d_chDataOffsets,
                                  uint32_t const* __restrict__ d_enabledErx,
                                  float const* __restrict__ d_sfLD,
                                  float const* __restrict__ d_sfHD,
                                  float const* __restrict__ d_adcPed,
                                  float const* __restrict__ d_cmPed,
                                  unsigned char const* __restrict__ d_valid,
                                  int const* __restrict__ d_ntoa,
                                  int const* __restrict__ d_ntot,
                                  uint32_t ndigis) const {
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

        // Mean-subtracted channel index: mean over the whole MODULE, chIdx unique per channel,
        // giving +/-110.5 on a 6-eRx LD module. Confirmed as the training convention by the
        // nchadc=222 field (the whole-module channel count, not the per-eRx 37) in the model
        // author's per-module constants record.
        slot.msubchidx() = float(chIdx) - (float(nErx) * 37.0f - 1.0f) / 2.0f;

        // Mean-subtracted eRx index (same for all channels in the same eRx).
        slot.msuberxidx() = float(erxIdx) - (float(nErx) - 1.0f) / 2.0f;

        // Cell area fraction: SF from cellareas.json, indexed by within-module chIdx.
        // isHD() selects the MH_F (HD, 444-entry) table; otherwise ML_F (LD, 222-entry).
        bool const isHD = cellmap_view[idxinfo.cellInfoIdx()].isHD();
        slot.cellfrac() = isHD ? d_sfHD[chIdx] : d_sfLD[chIdx];

        // Per-module scalars, identical for every channel in the same module.
        slot.ntoa() = float(d_ntoa[denseModIdx]);
        slot.ntot() = float(d_ntot[denseModIdx]);

        // CM sum per eRx (cm0..cm11): kCMScale * (0.5*cm - CM_ped), CM_ped from the first
        // channel of each eRx (chOffset + e*37). The scale is 2, i.e. the full-scale
        // cm - 2*CM_ped: the author's own pedestals (cm_erxNN) equal 2*CM_ped to within
        // 0.5 counts, so his feature cm_raw - cm_erxNN is exactly this. Inactive eRx stay 0.
        //
        // Dead/uncalibrated eRx are also zeroed. The calibration marks such channels
        // valid==0 with CM_ped==0; without this guard the raw CM sum would pass through
        // unsubtracted (O(500) counts vs the O(10) the DNN was trained on) and, because
        // cm0..cm11 are module-level features, that outlier would be broadcast to every
        // channel of the module. 0.0f is the same "this eRx contributes nothing" encoding
        // already used for inactive eRx, so it stays inside the training distribution.
#define FILL_CM(e_) \
        slot.cm##e_() = (((enabledErxMask >> uint32_t(e_)) & 1u) \
                         && d_valid[chOffset + uint32_t(e_) * 37u]) \
                        ? (kCMScale * (0.5f * float(digi_view[chOffset + uint32_t(e_) * 37u].cm()) \
                                      - d_cmPed[chOffset + uint32_t(e_) * 37u])) \
                        : 0.0f
        FILL_CM(0);  FILL_CM(1);  FILL_CM(2);  FILL_CM(3);
        FILL_CM(4);  FILL_CM(5);  FILL_CM(6);  FILL_CM(7);
        FILL_CM(8);  FILL_CM(9);  FILL_CM(10); FILL_CM(11);
#undef FILL_CM

        // 4 individual unconnected channel ADCs in this digi's eRx, pedestal-subtracted.
        // Within-eRx positions: 8, 17, 19, 28 (same for all LD modules).
        // CELL AREA SHOULD be 0 for unconnected channels Arne.
        // Uncalibrated channels (valid==0, ADC_ped==0) are zeroed for the same reason as the
        // CM slots above: without the guard the raw ADC (O(85) counts) would leak through
        // unsubtracted. Today those channels also happen to read adc==0 on a dead eRx, so the
        // result is 0 either way — but that is a coincidence of the eRx being empty, not a
        // property of valid==0.
        constexpr uint32_t kUnconn[4] = {8u, 17u, 19u, 28u};
#define FILL_UNCONN(j_) \
        slot.unconn##j_() = d_valid[chOffset + erxIdx * 37u + kUnconn[j_]] \
                            ? (float(digi_view[chOffset + erxIdx * 37u + kUnconn[j_]].adc()) \
                               - d_adcPed[chOffset + erxIdx * 37u + kUnconn[j_]]) \
                            : 0.0f
        FILL_UNCONN(0); FILL_UNCONN(1); FILL_UNCONN(2); FILL_UNCONN(3);
#undef FILL_UNCONN
      }
    }
  };

  // ---------------------------------------------------------------------------
  // Kernel: apply subtractive DNN correction to digi ADC in-place.
  // corrected_adc = raw_adc - model_prediction  (model predicts the noise)
  //
  // Unconnected channels are left untouched: they carry no sensor cell, so their
  // cell-area SF (cellfrac, from cellareas.json) is 0 and the DNN prediction is
  // meaningless for them. CM channels are not part of the digi collection at all
  // (37 entries/eRx = 36 data + 1 calib; CM lives in the per-digi .cm() field),
  // so they are never iterated here.
  // ---------------------------------------------------------------------------
  struct HGCalCMCalibKernel_applyCorrections {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  hgcaldigi::HGCalDigiDevice::View digi_view,
                                  HGCalCMCorrectionDeviceCollection::ConstView corr_view,
                                  HGCalSoACMMLDeviceCollection::ConstView mlsoa,
                                  uint32_t ndigis) const {
      for (auto idx : uniform_elements(acc, ndigis)) {
        // Skip unconnected channels (cell-area SF == 0): leave the raw ADC as-is.
        if (mlsoa[idx].cellfrac() == 0.0f)
          continue;
        float corrected = float(digi_view[idx].adc()) - corr_view[idx].correction();
        // Round, don't truncate. A C-style uint16_t conversion truncates toward zero, which for
        // the (non-negative) corrected ADC is a floor: it biases every corrected channel low by
        // ~0.5 counts. That bias is not removed by the RecHit pedestal subtraction downstream --
        // it survives into a systematic negative energy offset on every corrected channel.
        // Rounding leaves only the unavoidable +-0.5 quantization spread, with zero mean.
        digi_view[idx].adc() = uint16_t(std::clamp(std::round(corrected), 0.0f, 65535.0f));
      }
    }
  };

  // ---------------------------------------------------------------------------
  // fillCMInputs: fill ML input SoA from digis, one slot per digi.
  // ---------------------------------------------------------------------------
  void HGCalCMCalibrationAlgorithms::fillCMInputs(Queue& queue,
                                                   uint32_t ndigis,
                                                   int const* d_ntoa,
                                                   int const* d_ntot,
                                                   hgcaldigi::HGCalDigiDevice const& device_digis,
                                                   hgcal::HGCalDenseIndexInfoDevice const& device_index,
                                                   hgcal::HGCalMappingCellParamDevice const& device_cellmap,
                                                   uint32_t const* d_chDataOffsets,
                                                   uint32_t const* d_enabledErx,
                                                   float const* d_sfLD,
                                                   float const* d_sfHD,
                                                   float const* d_adcPed,
                                                   float const* d_cmPed,
                                                   unsigned char const* d_valid,
                                                   HGCalSoACMMLDeviceCollection& device_mlsoa) const {
    LogDebug("HGCalCMCalibrationAlgorithms") << "fillCMInputs: ndigis=" << ndigis;

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
                        d_sfLD,
                        d_sfHD,
                        d_adcPed,
                        d_cmPed,
                        d_valid,
                        d_ntoa,
                        d_ntot,
                        ndigis);
  }

  // ---------------------------------------------------------------------------
  // applyCMCorrections: subtract DNN prediction from digi ADC in-place.
  // ---------------------------------------------------------------------------
  void HGCalCMCalibrationAlgorithms::applyCMCorrections(
      Queue& queue,
      uint32_t ndigis,
      HGCalCMCorrectionDeviceCollection const& device_corrections,
      HGCalSoACMMLDeviceCollection const& device_mlsoa,
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
                        device_mlsoa.const_view(),
                        ndigis);
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE
