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

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ModuleFactory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "CondFormats/DataRecord/interface/HGCalMappingModuleIndexerRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHostCollection.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDeviceCollection.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcal {

    class HGCalMappingModuleESProducer : public ESProducer {
    public:

      //
      HGCalMappingModuleESProducer(const edm::ParameterSet& iConfig)
        : ESProducer(iConfig),
          filename_(iConfig.getParameter<std::string>("filename")) {
        auto cc = setWhatProduced(this);
        moduleIndexTkn_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("moduleindexer"));
      }
      
      //
      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<std::string>("filename", "Geometry/HGCalMapping/data/modulelocator.txt");
        desc.add<edm::ESInputTag>("moduleindexer",edm::ESInputTag(""))->setComment("Dense module index tool");
        descriptions.addWithDefaultLabel(desc);
      }

      //
      std::optional<HGCalMappingModuleParamHostCollection> produce(const HGCalMappingModuleIndexerRcd& iRecord) {

        //get module indexer
        auto cpi = iRecord.get(moduleIndexTkn_);
           
        // load dense indexing
        const uint32_t size = cpi.getSize(); // channel-level size
        HGCalMappingModuleParamHostCollection moduleParams(size, cms::alpakatools::host());

        // load module mapping parameters
        edm::FileInPath fip(filename_);
        std::ifstream file(fip.fullPath());
        std::string line;
        size_t iline(0);
        bool isSiPM,isHD;
        int plane, u, v, zside;
        uint16_t fedid,localfedid,wafType,captureblock,econdidx,captureblockidx;
        while(std::getline(file, line))
        {
          iline++;
          if(iline==1) continue;
          std::istringstream stream(line);
          stream >> plane >> u >> v >> isSiPM >> isHD >> wafType >> econdidx >> captureblock >> localfedid >> captureblockidx >> fedid >> zside;

          uint32_t idx = cpi.denseIndex(localfedid, captureblock, econdidx);
          moduleParams.view()[idx].zside()             = (zside>0);
          moduleParams.view()[idx].isSiPM()            = isSiPM;
          moduleParams.view()[idx].isHD()              = isHD;
          moduleParams.view()[idx].plane()             = plane;
          moduleParams.view()[idx].u()                 = u;
          moduleParams.view()[idx].v()                 = v;
          moduleParams.view()[idx].fedid()             = fedid;
          moduleParams.view()[idx].localfedid()        = localfedid;
          moduleParams.view()[idx].wafType()           = wafType;
          moduleParams.view()[idx].captureblock()      = captureblock;
          moduleParams.view()[idx].econdidx()          = econdidx;
          moduleParams.view()[idx].captureblockidx()   = captureblockidx;
        }

        return moduleParams;
      }  // end of produce()

    private:
      edm::ESGetToken<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd> moduleIndexTkn_;
      const std::string filename_;
    };

  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalMappingModuleESProducer);
