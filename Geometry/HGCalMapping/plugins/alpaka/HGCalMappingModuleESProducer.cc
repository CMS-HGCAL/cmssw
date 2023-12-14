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

        //get cell and module indexer
        auto modidx = iRecord.get(moduleIndexTkn_);

        
        // load dense indexing
        const uint32_t size = modidx.maxModulesIdx_;
        HGCalMappingModuleParamHostCollection moduleParams(size, cms::alpakatools::host());

        // load module mapping parameters
        edm::FileInPath fip(filename_);
        std::ifstream file(fip.fullPath());
        std::string line, typecode;
        size_t iline(0);
        int plane, u, v, zside;
        uint16_t fedid,slinkidx,captureblock,econdidx,captureblockidx;
        while(std::getline(file, line))
          {
            iline++;
            if(iline==1) continue;
            
            std::istringstream stream(line);
            stream >> plane >> u >> v >> typecode >> econdidx >> captureblock >> captureblockidx >> slinkidx >> fedid >> zside;

            /*
            uint32_t typecodeIdx = cellIndexer_.getEnumFromTypecode("MH-F");
            if(typecode.find("M")==0 && typecode.size()>4) typecode = typecode.substr(0,4);
            try {
              typecodeIdx = cellIndexer_.getEnumFromTypecode(typecode);
            }catch(cms::Exception &e) {
              edm::LogWarning("HGCalMappingIndexESSource") << "Exception caught decoding index for typecode=" << typecode
                                                           << " @ plane=" << plane << " u=" << u << " v=" << v << "\n"
                                                           << e.what() << "\n"
                                                           << "===> will assign default (MH-F) which may be inefficient";
            }
            

            
        
              
              isHD = false;
              if(typecode[0] == 'T') {
              isSiPM = true;
              type = modidx.convertSiPMTypecode(typecode);
              }
              else {
              isSiPM = false;
              if(typecode[1] == 'H'){
              isHD = true;
              }
              type = modidx.convertSiTypecode(typecode);
              thickness = atoi(&typecode[4]);
              }
              
              uint32_t idx = modidx.denseIndex(fedid, captureblock, econdidx);
              moduleParams.view()[idx].zside()             = (zside>0);
              moduleParams.view()[idx].isSiPM()            = isSiPM;
              moduleParams.view()[idx].isHD()              = isHD;
              moduleParams.view()[idx].plane()             = plane;
              moduleParams.view()[idx].u()                 = u;
              moduleParams.view()[idx].v()                 = v;
              moduleParams.view()[idx].thickness()         = thickness;
              moduleParams.view()[idx].type()              = type;
              moduleParams.view()[idx].fedid()             = fedid;
              moduleParams.view()[idx].slinkidx()          = slinkidx;
              moduleParams.view()[idx].captureblock()      = captureblock;
              moduleParams.view()[idx].econdidx()          = econdidx;
              moduleParams.view()[idx].captureblockidx()   = captureblockidx;
            */
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
