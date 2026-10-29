#include <cstddef>
#include <sstream>
#include <cmath>

// CMSSW imports
#include "FWCore/MessageLogger/interface/MessageLogger.h"

// Alpaka imports
#include "HeterogeneousCore/AlpakaInterface/interface/traits.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

// HGCal imports
#include "SimCalorimetry/HGCalSimProducers/interface/alpaka/HGCalDigitizerAlgorithms.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  // @short this kernel takes care of adding noise on each bunch crossing
  struct HGCalROCDigitizerKernel_addNoise {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  HGCalRealisticSimHitDevice::View realisticHits,
                                  HGCalAgeingCondsDevice::ConstView conds,
                                  HGCalDenseIndexInfoDevice::ConstView denseIndex,
                                  HGCalMappingCellParamDevice::ConstView cellInfo) const {
      // helper to add noise for each time slice
      auto addNoise = [&](auto sumQ, float noise) {
        for (auto j = 0; j < hgcaldigi::kTimeBins; j++) {
          //@Christian noise here is fixed
          sumQ(j, 0) = noise + sumQ(j, 0);
        }
      };  //end noise loop

      // dispatch operation
      for (auto idx : uniform_elements(acc, realisticHits.metadata().size())) {
        auto hit = realisticHits[idx];
        //auto cellIdx = denseIndex[idx].cellInfoIdx();
        //bool isSiPM = cellInfo[cellIdx].isSiPM();
        auto enc = conds[idx].enc();
        addNoise(hit.sumQ(), enc);
      }  //end grid loop
    }
  };

  // @short this kernel takes care of finalizing the Geant4 energy response and assigning the final energy deposit and average time estimates
  struct HGCalROCDigitizerKernel_digitize {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  HGCalROCDigisAccumDevice::View rocDigis,
                                  HGCalRealisticSimHitDevice::ConstView realisticHits,
                                  HGCalAgeingCondsDevice::ConstView conds,
                                  HGCalDenseIndexInfoDevice::ConstView denseIndex,
                                  HGCalMappingCellParamDevice::ConstView cellInfo) const {
      auto quantize = [](float value, float lsb, int maxCode) -> uint32_t {
        if (!(lsb > 0.f))
          return 0u;
        int code = static_cast<int>(std::round(value / lsb));
        if (!(code > 0.f))
          return 0u;
        return static_cast<uint32_t>(code < 0 ? 0 : (code > maxCode ? maxCode : code));
      };

      // build the energy part of the word
      auto digitizeEn =
          [&](auto word, const auto sumQ, const float& adc_lsb, const float& tot_lsb, const float& tot_thr) {
            for (auto j = 0; j < hgcaldigi::kTimeBins; j++) {
              auto adc = quantize(sumQ(j, 0), adc_lsb, 1023);

              //FIXME this is a placeholder
              word(j, 0) = word(j, 0) | adc;

              if (j == hgcaldigi::kTimeBins - 1)
                continue;
              word(j + 1, 0) = word(j, 0) | (adc << 12);
            }
          };

      // build the time part of the word
      auto digitizeTOA = [&](auto word, const auto avgTOA) {
        constexpr float toa_lsb = 25. / 1024;
        for (auto j = 0; j < hgcaldigi::kTimeBins; j++) {
          auto toa = avgTOA(j, 0);
          word(j, 0) = word(j, 0) | quantize(toa, toa_lsb, 1023);
        }
      };

      // dispatch operation
      for (auto idx : uniform_elements(acc, realisticHits.metadata().size())) {
        auto digi = rocDigis[idx];
        const auto hit = realisticHits[idx];
        digitizeEn(digi.word(), hit.sumQ(), conds[idx].adc_lsb(), conds[idx].tot_lsb(), conds[idx].tot_thr());
        digitizeTOA(digi.word(), hit.avgTOA());
      }  //end grid loop
    }
  };

  // @short apply all calibration kernels
  HGCalROCDigisAccumDevice HGCalROCDigitizerAlgorithms::digitize(Queue& queue,
                                                                 HGCalRealisticSimHitDevice& device_realisticHits,
                                                                 HGCalAgeingCondsDevice const& ageConds,
                                                                 HGCalDenseIndexInfoDevice const& denseIndex,
                                                                 HGCalMappingCellParamDevice const& cellInfo) const {
    edm::LogWarning("HGCalROCDigitizerAlgorithms") << "Start of digitization" << std::endl;
    auto const ncells = device_realisticHits.view().metadata().size();
    HGCalROCDigisAccumDevice device_rocDigis(queue, ncells);

    // number of items per group and division in groups to cover the whole problem
    uint32_t items = n_threads_;
    uint32_t groups = divide_up_by(ncells, items);

    // generate the working grid
    //   - threads with a single element per thread on a GPU backend
    //   - elements within a single thread on a CPU backend
    auto grid = make_workdiv<Acc1D>(groups, items);
    edm::LogWarning("HGCalROCDigitizerAlgorithms") << "N groups: " << groups << "\tN items: " << items << std::endl;

    //execute digitization kernels
    //@Christian : maybe here is a place to pass seeds
    alpaka::exec<Acc1D>(queue,
                        grid,
                        HGCalROCDigitizerKernel_addNoise{},
                        device_realisticHits.view(),
                        ageConds.const_view(),
                        denseIndex.const_view(),
                        cellInfo.const_view());

    alpaka::exec<Acc1D>(queue,
                        grid,
                        HGCalROCDigitizerKernel_digitize{},
                        device_rocDigis.view(),
                        device_realisticHits.const_view(),
                        ageConds.const_view(),
                        denseIndex.const_view(),
                        cellInfo.const_view());

    //FIXME - uncomment ifdef for final version
    //#ifdef EDM_ML_DEBUG
    int n_hits_to_print = 10;
    alpaka::exec<Acc1D>(queue, grid, HGCalAgeingKernel_printHits{}, device_realisticHits.view(), n_hits_to_print);
    //#endif

    return device_rocDigis;
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE
