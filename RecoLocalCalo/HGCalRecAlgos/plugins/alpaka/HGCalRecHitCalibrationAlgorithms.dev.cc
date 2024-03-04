// alpaka-related imports
#include <alpaka/alpaka.hpp>
#include "HeterogeneousCore/AlpakaInterface/interface/traits.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

// Calibration algorithms header
#include "RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalRecHitCalibrationAlgorithms.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  enum HGCalCalibrationFlag {
      kPedestalCorrection=0,
      kCMCorrection,
      kADCmCorrection,
  };

  class HGCalRecHitCalibrationKernel_digisToRecHits {
  public:

    template <typename TAcc>

    ALPAKA_FN_ACC void operator()(TAcc const& acc, HGCalDigiDeviceCollection::View digis, HGCalRecHitDeviceCollection::View recHits) const {
      auto ToA_to_time = [&](uint32_t ToA) { return float(ToA)*0.024414062; }; // LSB=25 ns / 2^10b
      auto ADC_to_float = [&](uint32_t ADC, uint32_t TOT, uint8_t tctp) { return float(tctp>1 ? TOT : ADC); };
      
      // dummy digis -> rechits conversion (to be replaced by the actual formula)
      for (auto index : elements_with_stride(acc, digis.metadata().size())) {
        recHits[index].detid() = static_cast<uint32_t>(digis[index].electronicsId());
        recHits[index].energy() = ADC_to_float(digis[index].adc(),digis[index].tot(),digis[index].tctp());
        recHits[index].time() = ToA_to_time(digis[index].toa());
        recHits[index].flags() = digis[index].flags();
      }
    }
  };

  struct HGCalRecHitCalibrationKernel_pedestalCorrection {
    template <typename TAcc>
    ALPAKA_FN_ACC void operator()(TAcc const& acc, HGCalDigiDeviceCollection::View digis, HGCalRecHitDeviceCollection::View recHits, HGCalCalibParamDeviceCollection::ConstView calib) const {
      auto const& config_calib_param = calib.config();
      for (auto index : elements_with_stride(acc, digis.metadata().size())) {
        if ((digis[index].tctp()==0) && (digis[index].flags() >> kPedestalCorrection) & 1){
          uint32_t idx = config_calib_param.denseMap(digis[index].electronicsId());
          float pedestalValue = calib[idx].pedestal();
          recHits[index].energy() = recHits[index].energy() - pedestalValue;
        }
      }
    }
  };

  struct HGCalRecHitCalibrationKernel_chargeConversion {
    template <typename TAcc>
    ALPAKA_FN_ACC void operator()(TAcc const& acc, HGCalDigiDeviceCollection::View digis, HGCalRecHitDeviceCollection::View recHits, HGCalConfigParamDeviceCollection::ConstView config) const {
      auto const& config_param = config.config();
      auto ADC_to_charge = [&](float energy, uint8_t tctp, uint8_t gain) {
        return tctp>1 ? energy*1.953125 : energy*gain*0.078125; // fC
        //                TOT / 2^12 * 8000 fC = TOT * 1.953125 fC
        // ( ADC - pedestal ) / 2^10 *   80 fC = ( ADC - pedestal ) * 0.078125 fC
      };
      for (auto index : elements_with_stride(acc, digis.metadata().size())) {
        uint32_t idx = config_param.denseROCMap(digis[index].electronicsId());
        recHits[index].energy() = ADC_to_charge(recHits[index].energy(),digis[index].tctp(),config[idx].gain());
      }
    }
  };

  struct HGCalRecHitCalibrationKernel_commonModeCorrection {
    template <typename TAcc>
    ALPAKA_FN_ACC void operator()(TAcc const& acc, HGCalDigiDeviceCollection::View digis, HGCalRecHitDeviceCollection::View recHits, HGCalCalibParamDeviceCollection::ConstView calib) const {
      auto const& config_calib_param = calib.config();
      for (auto index : elements_with_stride(acc, recHits.metadata().size())) {
        uint32_t idx = config_calib_param.denseMap(digis[index].electronicsId());
        float commonModeValue = calib[idx].CM_slope() * digis[index].cm() + calib[idx].CM_offset();
        recHits[index].energy() -= commonModeValue;
      }
    }
  };

  struct HGCalRecHitCalibrationKernel_ADCmCorrection {
    template <typename TAcc>
    ALPAKA_FN_ACC void operator()(TAcc const& acc, HGCalDigiDeviceCollection::View digis, HGCalRecHitDeviceCollection::View recHits, HGCalCalibParamDeviceCollection::ConstView calib) const {
      auto const& config_calib_param = calib.config();
      for (auto index : elements_with_stride(acc, recHits.metadata().size())) {
        uint32_t idx = config_calib_param.denseMap(digis[index].electronicsId());
        float ADCmValue = calib[idx].BXm1_slope() * digis[index].adcm1() + calib[idx].BXm1_offset(); // placeholder
        recHits[index].adc() -= ADCmValue;
      }
    }
  };

  struct HGCalRecHitCalibrationKernel_printRecHits {
    template <typename TAcc>
    ALPAKA_FN_ACC void operator()(TAcc const& acc, HGCalRecHitDeviceCollection::ConstView view, int size) const {
      for (int i = 0; i < size; ++i) {
#ifdef EDM_ML_DEBUG
        auto const& recHit = view[i];
        printf("%d\t%d\t%f\t%f\t%d\n", i, recHit.detid(), recHit.energy(), recHit.time(), recHit.flags());
#endif
      }
    }
  };

  void HGCalRecHitCalibrationAlgorithms::loadCalibParams(CalibParams& newCalibParams) {
    LogDebug("HGCalRecHitCalibrationAlgorithms") << "\nINFO -- HGCalRecHitCalibrationAlgorithms::loadCalibParams: " << newCalibParams.size() << " elements" << std::endl;
    calibParams = newCalibParams;
  }

  std::unique_ptr<HGCalRecHitDeviceCollection> HGCalRecHitCalibrationAlgorithms::calibrate(
      Queue& queue, HGCalDigiHostCollection const& host_digis,
      HGCalCalibParamDeviceCollection const& device_calib, HGCalConfigParamDeviceCollection const& device_config
    ) {

    LogDebug("HGCalRecHitCalibrationAlgorithms") << "\n\nINFO -- Start of calibrate\n\n" << std::endl;
    LogDebug("HGCalRecHitCalibrationAlgorithms") << "N blocks: " << n_blocks << "\tN threads: " << n_threads << std::endl;
    auto grid = make_workdiv<Acc1D>(n_blocks, n_threads);

    LogDebug("HGCalRecHitCalibrationAlgorithms") << "\n\nINFO -- copying the digis to the device\n\n" << std::endl;
    HGCalDigiDeviceCollection device_digis(host_digis.view().metadata().size(), queue);
    alpaka::memcpy(queue, device_digis.buffer(), host_digis.const_buffer());

    LogDebug("HGCalRecHitCalibrationAlgorithms") << "\n\nINFO -- allocating rechits buffer and initiating values" << std::endl;
    auto device_recHits = std::make_unique<HGCalRecHitDeviceCollection>(device_digis.view().metadata().size(), queue);
    alpaka::exec<Acc1D>(queue, grid, HGCalRecHitCalibrationKernel_digisToRecHits{}, device_digis.view(), device_recHits->view());
    LogDebug("HGCalRecHitCalibrationAlgorithms") << "Input recHits: " << std::endl;
    int n_hits_to_print = 10;
    print_recHit_device(queue, *device_recHits, n_hits_to_print);

    alpaka::exec<Acc1D>(queue, grid, HGCalRecHitCalibrationKernel_pedestalCorrection{}, device_digis.view(), device_recHits->view(), device_calib.view());
    LogDebug("HGCalRecHitCalibrationAlgorithms") << "RecHits after pedestal calibration: " << std::endl;
    print_recHit_device(queue, *device_recHits, n_hits_to_print);

    alpaka::exec<Acc1D>(queue, grid, HGCalRecHitCalibrationKernel_commonModeCorrection{}, device_digis.view(), device_recHits->view(), device_calib.view());
    LogDebug("HGCalRecHitCalibrationAlgorithms") << "Digis after CM calibration: " << std::endl;
    //print_digi_device(device_digis, n_hits_to_print);
    print_recHit_device(queue, *device_recHits, n_hits_to_print);

    alpaka::exec<Acc1D>(queue, grid, HGCalRecHitCalibrationKernel_chargeConversion{}, device_digis.view(), device_recHits->view(), device_config.view());
    LogDebug("HGCalRecHitCalibrationAlgorithms") << "RecHits after charge conversion: " << std::endl;
    print_recHit_device(queue, *device_recHits, n_hits_to_print);

    /*
    float ADCmValue = n_hits_to_print; // dummy value
    alpaka::exec<Acc1D>(queue, grid, HGCalRecHitCalibrationKernel_ADCmCorrection{}, device_digis.view(), calib_device.view());
    LogDebug("HGCalRecHitCalibrationAlgorithms") << "Digis after ADCm calibration: " << std::endl;
    print_digi_device(device_digis, n_hits_to_print);

    LogDebug("HGCalRecHitCalibrationAlgorithms") << "RecHits after calibration: " << std::endl;
    print_recHit_device(queue, *device_recHits, n_hits_to_print);
    */

    return device_recHits;
  }

  void HGCalRecHitCalibrationAlgorithms::print(HGCalDigiHostCollection const& digis, int max) const {
    int max_ = max > 0 ? max : digis.view().metadata().size();
    for (int i = 0; i < max_; i++) {
      LogDebug("HGCalRecHitCalibrationAlgorithms") << i
        << "\t" << digis.view()[i].electronicsId()
        << "\t" << digis.view()[i].tctp()
        << "\t" << digis.view()[i].adcm1()
        << "\t" << digis.view()[i].adc()
        << "\t" << digis.view()[i].tot()
        << "\t" << digis.view()[i].toa()
        << "\t" << digis.view()[i].cm()
        << "\t" << digis.view()[i].flags()
        << std::endl;
    }
  }

  void HGCalRecHitCalibrationAlgorithms::print_digi_device(HGCalDigiDeviceCollection const& digis, int max) const {
    int max_ = max > 0 ? max : digis.view().metadata().size();
    for (int i = 0; i < max_; i++) {
      LogDebug("HGCalRecHitCalibrationAlgorithms") << i
        << "\t" << digis.view()[i].electronicsId()
        << "\t" << digis.view()[i].tctp()
        << "\t" << digis.view()[i].adcm1()
        << "\t" << digis.view()[i].adc()
        << "\t" << digis.view()[i].tot()
        << "\t" << digis.view()[i].toa()
        << "\t" << digis.view()[i].cm()
        << "\t" << digis.view()[i].flags()
        << std::endl;
    }
  }

  void HGCalRecHitCalibrationAlgorithms::print_recHit_device(Queue& queue, HGCalRecHitDeviceCollection const& recHits, int max) const {
    auto grid = make_workdiv<Acc1D>(1, 1);
    auto size = max > 0 ? max : recHits.view().metadata().size();
    alpaka::exec<Acc1D>(queue, grid, HGCalRecHitCalibrationKernel_printRecHits{}, recHits.view(), size);
    // ensure that the print operations are complete before returning
    alpaka::wait(queue);
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

//
// Some potentially useful code snippets:
//

//   void HGCalRecHitCalibrationAlgorithms::fill(Queue& queue, HGCalRecHitDeviceCollection& collection, double xvalue) const {
//     // use 64 items per group (this value is arbitrary, but it's a reasonable starting point)
//     uint32_t items = 64;

//     // use as many groups as needed to cover the whole problem
//     uint32_t groups = divide_up_by(collection->metadata().size(), items);

//     // map items to
//     // -threadswithasingleelementperthreadonaGPUbackend
//     // -elementswithinasinglethreadonaCPUbackend
//     auto workDiv = make_workdiv<Acc1D>(groups, items);

//     alpaka::exec<Acc1D>(queue, workDiv, HGCalRecHitCalibrationKernel{}, collection.view(), collection->metadata().size(), xvalue);
//   }
