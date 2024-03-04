#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/SourceFactory.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESProducer.h"
#include "FWCore/Framework/interface/ESTransientHandle.h"
#include "FWCore/Framework/interface/EventSetupRecordIntervalFinder.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/ESGetToken.h"
#include "DataFormats/Math/interface/libminifloat.h"

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ModuleFactory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableConfigRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableConfig.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalCalibrationParameterIndex.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalCalibrationParameterHostCollection.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCalibrationParameterDeviceCollection.h"

#include <string>
#include <iostream> // for std::cout
#include <iomanip> // for std::setw

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcalrechit {

    class HGCalConfigurationESProducer : public ESProducer {
    public:

      HGCalConfigurationESProducer(const edm::ParameterSet& iConfig)
        : ESProducer(iConfig),
          //charMode_(iConfig.getParameter<int>("charMode")),
          gain_(iConfig.getParameter<int>("gain")) {
        auto cc = setWhatProduced(this);
        //findingRecord<HGCalCondSerializableConfigRcd>();
        configToken_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("configSource"));
        moduleInfoToken_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("configSource"));
      }

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<edm::ESInputTag>("configSource",edm::ESInputTag(""))->setComment("Label for ROC configuration parameters");
        //desc.add<int>("charMode",-1)->setComment("Manual override for characterization mode to unpack raw data");
        desc.add<int>("gain",-1)->setComment("Manual override for gain (1: 80 fC, 2: 160 fC, 4: 320 fC)");
        descriptions.addWithDefaultLabel(desc);
      }

      std::optional<hgcalrechit::HGCalConfigParamHostCollection> produce(const HGCalCondSerializableConfigRcd& iRecord) {
        const auto& config = iRecord.get(configToken_);
        const auto& moduleInfo = iRecord.get(moduleInfoToken_);

        // load dense indexing
        HGCalCalibrationParameterIndex cpi;
        cpi.setMaxValues(moduleInfo);
        const uint32_t size = cpi.getSize(true); // ROC-level size
        hgcalrechit::HGCalConfigParamHostCollection product(size, cms::alpakatools::host());
        product.view().config() = cpi; // set dense indexing in SoA

        // fill SoA
        size_t nmods = config.moduleConfigs.size();
          LogDebug("HGCalRecHitCalibrationAlgorithms") << "Configuration retrieved for " << nmods << " modules: " << config << std::endl;
        for(auto it : config.moduleConfigs) { // loop over map module electronicsId -> HGCalModuleConfig
          HGCalModuleConfig moduleConfig(it.second);
          LogDebug("HGCalRecHitCalibrationAlgorithms")
            << "Module " << it.first << std::hex << " (0x" << it.first << std::dec
            << ") charMode=" << moduleConfig.charMode
            << ", ngains=" << moduleConfig.gains.size(); //<< std::endl;
          for(auto rocit : moduleConfig.gains) {
            uint32_t rocid = rocit.first;
            uint8_t gain = (gain_>=1 ? gain_ : rocit.second); // allow manual override
            product.view()[cpi.denseROCMap(rocid)].gain() = gain;
            LogDebug("HGCalRecHitCalibrationAlgorithms")
              << "  ROC " << std::setw(4) << rocid << std::hex << " (0x" << rocid << std::dec
              << "): gain=" << (unsigned int) gain << " (override: " << gain_ << ")"; //std::endl;
          }
        }

        return product;
      }  // end of produce()

    private:
      edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
      edm::ESGetToken<HGCalCondSerializableConfig, HGCalCondSerializableConfigRcd> configToken_;
      //const int charMode_; // manual override of YAML files
      const int gain_; // manual override of YAML files

    };

  }  // namespace hgcalrechit

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcalrechit::HGCalConfigurationESProducer);
