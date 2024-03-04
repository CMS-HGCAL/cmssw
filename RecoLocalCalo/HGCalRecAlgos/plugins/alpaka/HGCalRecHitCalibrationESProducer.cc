#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/SourceFactory.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESProducer.h"
#include "FWCore/Framework/interface/ESProducts.h"
#include "FWCore/Framework/interface/ESTransientHandle.h"
#include "FWCore/Framework/interface/EventSetupRecordIntervalFinder.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
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

#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalCalibrationParameterIndex.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalCalibrationParameterHostCollection.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCalibrationParameterDeviceCollection.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcalrechit {

    class HGCalCalibrationESProducer : public ESProducer {
    public:

      HGCalCalibrationESProducer(const edm::ParameterSet& iConfig)
        : ESProducer(iConfig),
          filename_(iConfig.getParameter<std::string>("filename")) {
        auto cc = setWhatProduced(this);
        moduleInfoToken_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("moduleInfoSource"));
      }

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<std::string>("filename", {});
        desc.add<edm::ESInputTag>("moduleInfoSource",edm::ESInputTag(""))->setComment("Label for module info to calculate size");
        descriptions.addWithDefaultLabel(desc);
      }

      std::optional<hgcalrechit::HGCalCalibParamHostCollection> produce(const HGCalCondSerializableModuleInfoRcd& iRecord) {
        auto const& moduleInfo = iRecord.get(moduleInfoToken_);

        // load dense indexing
        HGCalCalibrationParameterIndex cpi;
        cpi.setMaxValues(moduleInfo);
        const uint32_t size = cpi.getSize(); // channel-level size
        hgcalrechit::HGCalCalibParamHostCollection product(size, cms::alpakatools::host());
        product.view().config() = cpi; // set dense indexing in SoA

        // load calib parameters
        edm::FileInPath fip(filename_);
        std::ifstream file(fip.fullPath());
        std::string line;
        uint32_t id;
        float ped, noise, cm_slope, cm_offset, bxm1_slope, bxm1_offset;
        while(std::getline(file, line)) {
          if(line.find("Channel")!=std::string::npos || line.find("#")!=std::string::npos) continue;

          std::istringstream stream(line);
          stream >> std::hex >> id >> std::dec >> ped >> noise >> cm_slope >> cm_offset >> bxm1_slope >> bxm1_offset;

          //reduce to half-point float and fill the pedestals of this channel
          uint32_t idx = cpi.denseMap(id); // convert electronicsId to idx from denseMap

          // Comment: if planning to use MiniFloatConverter::float32to16(), a host function,
          // one needs to think how to perform MiniFloatConverter::float16to32() in kernels running on GPU (HGCalCalibrationAlgorithms.dev.cc)
          product.view()[idx].pedestal()    = ped;
          product.view()[idx].CM_slope()    = cm_slope;
          product.view()[idx].CM_offset()   = cm_offset;
          product.view()[idx].BXm1_slope()  = bxm1_slope;
          product.view()[idx].BXm1_offset() = bxm1_offset;
        }

        return product;
      }  // end of produce()

    private:
      edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
      const std::string filename_;
    };

  }  // namespace hgcalrechit

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcalrechit::HGCalCalibrationESProducer);
