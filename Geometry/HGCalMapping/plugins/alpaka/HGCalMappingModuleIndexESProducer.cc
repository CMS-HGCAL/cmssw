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

#include "Geometry/HGCalMapping/interface/HGCalMappingParameterIndex.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingParameterHostCollection.h"
#include "Geometry/HGCalMapping/interface/alpaka/HGCalMappingParameterDeviceCollection.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcal {

    class HGCalMappingIndexESProducer : public ESProducer {
    public:

      HGCalMappingIndexESProducer(const edm::ParameterSet& iConfig)
        : ESProducer(iConfig),
          filename_(iConfig.getParameter<std::string>("filename")) {
      }

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<std::string>("filename", {});
        descriptions.addWithDefaultLabel(desc);
      }

      std::optional<HGCalMappingParameterIndex> produce(const HGCalCondSerializableModuleInfoRcd& iRecord) {

        // load module mapping parameters
        edm::FileInPath fip(filename_);
        std::ifstream file(fip.fullPath());
        std::string line;
        size_t iline(0);
        bool isSiPM,isHD;
        int plane, u, v, zside;
        uint16_t fedid,slink,wafType,captureblock,econdidx,captureblockidx;

        uint16_t maxslink(0),maxcaptureblock(0),maxecondidx(0),maxerx(0);

        while(std::getline(file, line))
        {
          iline++;
          if(iline==1) continue;
          
          std::istringstream stream(line);
          stream >> plane >> u >> v >> isSiPM >> isHD >> wafType >> econdidx >> captureblock >> slink >> captureblockidx >> fedid >> zside;

          maxslink=std::max(slink,maxslink);
          maxcaptureblock=std::max(captureblock,maxcaptureblock);
          maxecondidx=std::max(econdidx,maxecondidx);
          uint16_t nerx=6*(1+isHD);
          maxerx=std::max(nerx,maxerx);
        }

        // load dense indexing
        HGCalMappingParameterIndex cpi;
        cpi.setMaxValues(maxslink, maxcaptureblock, maxecondidx, maxerx);


        return cpi;
      }  // end of produce()

    private:
      const std::string filename_;
    };

  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalMappingIndexESProducer);