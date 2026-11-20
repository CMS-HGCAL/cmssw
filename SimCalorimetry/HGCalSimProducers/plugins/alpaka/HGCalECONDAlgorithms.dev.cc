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
#include "DataFormats/HGCalDigi/interface/HGCalRawDataDefinitions.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  // @short fills the common-mode (CM) channels of the ECON-Ds
  //   CM = CM_ped of the calibration + Gaussian noise of a CM channel (capacitance noise only, no leakage current)
  //   CM0 and CM1 get independent noise: no coherent shift is simulated
  //   one dense-index row per eRx does the work: the first channel of the eRx (chNumber % 37 == 0)
  struct HGCalECONDAlgosKernel_fillCM {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  HGCalECONDPacketInfoDevice::View econdInfo,
                                  HGCalMappingModuleParamDevice::ConstView moduleInfo,
                                  HGCalAgeingCondsDevice::ConstView conds,
                                  HGCalDenseIndexInfoDevice::ConstView denseIndex,
                                  hgcalrechit::HGCalCalibParamDevice::ConstView calibs,
                                  const HGCalCMNoiseParams cmParams,
                                  const uint64_t seed) const {
      // First a uniform integer as seed to the normal one. Otherwise matching with 
      // HGCalROCDigitizer seed inducin correlation.
      // One engine per thread (or per work item)
      const uint32_t globalThreadIdx = alpaka::getIdx<alpaka::Grid, alpaka::Threads>(acc)[0u];
      auto engine_ = alpaka::rand::engine::createDefault(acc, seed + globalThreadIdx, 0);
      auto uniform_ = alpaka::rand::distribution::createUniformUint<uint64_t>(acc);
      auto seed_ = uniform_(engine_);

      auto engine = alpaka::rand::engine::createDefault(acc, seed_, 0);
      auto normal = alpaka::rand::distribution::createNormalReal<float>(acc);

      constexpr uint32_t nChPerErx = HGCalMappingCellIndexer::maxChPerErx_;
      constexpr float maxCM = hgcal::ECOND_FRAME::COMMONMODE0_MASK;  // CM words are 10 bits
      const uint32_t ncells = denseIndex.metadata().size();
      const uint32_t ncalibs = calibs.metadata().size();
      const uint32_t nrows = ncells < ncalibs ? ncells : ncalibs;

      // dispatch operation
      for (auto idx : uniform_elements(acc, ncells)) {
        // only the first channel of each eRx fills the CM of that eRx
        const uint32_t chNumber = denseIndex[idx].chNumber();
        if (chNumber % nChPerErx != 0)
          continue;
        const uint32_t erx = chNumber / nChPerErx;
        const uint32_t modIdx = denseIndex[idx].modInfoIdx();

        // CM pedestal (CM_ped of the calibration, the same value the reco subtracts) and CM noise (from the gain)
        // taken from the first connected channel of the eRx
        // (the channels of an eRx are contiguous in the dense index: idx ... idx+36)
        // SiPM modules: pedestal only (no SiPM CM noise model yet); eRx without connected channels: 0
        float pedestal = 0.f;
        float noise = 0.f;
        const uint32_t last = (idx + nChPerErx < nrows) ? idx + nChPerErx : nrows;
        for (uint32_t k = idx; k < last; k++) {
          if (conds[k].adc_lsb() > 0.f) {
            pedestal = calibs[k].CM_ped();
            const auto gain = conds[k].gain();
            if (!moduleInfo[modIdx].isSiPM() && gain < HGCalCMNoiseParams::kNGains)
              noise = cmParams.noiseADC[gain];
            break;
          }
        }

        // CM0 and CM1: pedestal + independent noise, rounded and clamped to 10 bits
        auto econd = econdInfo[modIdx];
        for (int j = 0; j < 2; j++) {
          float val = pedestal + noise * normal(engine);
          val = val < 0.f ? 0.f : (val > maxCM ? maxCM : val);
          econd.cm()(erx, j) = static_cast<uint16_t>(val + 0.5f);
        }
      }  //end grid loop
    }
  };

  // @short apply the sequence of digitization kernels
  HGCalECONDPacketInfoDevice HGCalECONDAlgorithms::init(Queue& queue,
                                                        HGCalAgeingCondsDevice const& ageConds,
                                                        HGCalMappingModuleParamDevice const& moduleInfo,
                                                        HGCalDenseIndexInfoDevice const& denseIndex,
                                                        hgcalrechit::HGCalCalibParamDevice const& calibs,
                                                        HGCalCMNoiseParams const& cmParams,
                                                        const uint64_t seed) const {
    LogDebug("HGCalECONDAlgorithms") << "Start of ECON-D info" << std::endl;
    auto const necons = moduleInfo.view().metadata().size();
    HGCalECONDPacketInfoDevice device_econds(queue, necons);

    // start from zero: the CM rows of eRx not present in a module are not filled by the kernel
    device_econds.zeroInitialise(queue);

    // one element per cell (dense index): only the first channel of each eRx fills the CM
    auto const ncells = denseIndex.view().metadata().size();

    // number of items per group and division in groups to cover the whole problem
    uint32_t items = n_threads_;
    uint32_t groups = divide_up_by(ncells, items);

    // generate the working grid
    //   - threads with a single element per thread on a GPU backend
    //   - elements within a single thread on a CPU backend
    auto grid = make_workdiv<Acc1D>(groups, items);
    LogDebug("HGCalECONDAlgorithms") << "N groups: " << groups << "\tN items: " << items << std::endl;

    //execute digitization kernels
    alpaka::exec<Acc1D>(queue,
                        grid,
                        HGCalECONDAlgosKernel_fillCM{},
                        device_econds.view(),
                        moduleInfo.const_view(),
                        ageConds.const_view(),
                        denseIndex.const_view(),
                        calibs.const_view(),
                        cmParams,
                        seed);

    return device_econds;
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE
