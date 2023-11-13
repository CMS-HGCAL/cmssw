#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/SourceFactory.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"
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

#include "CondFormats/DataRecord/interface/HGCalMappingSiCellIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiPMCellIndexerRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingParameterHostCollection.h"
#include "Geometry/HGCalMapping/interface/alpaka/HGCalMappingParameterDeviceCollection.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcal {

    template<class R>
    class HGCalMappingCellESProducer : public ESProducer {
    public:

      //
      HGCalMappingCellESProducer(const edm::ParameterSet& iConfig)
        : ESProducer(iConfig),
          filename_(iConfig.getParameter<std::string>("filename")) {
        auto cc = setWhatProduced(this);
        cellIndexTkn_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("cellindexer"));
      }

      //
      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<std::string>("filename", {});
        desc.add<edm::ESInputTag>("cellindexer",edm::ESInputTag(""))->setComment("Dense cell index tool");
        descriptions.addWithDefaultLabel(desc);
      }

      //
      std::optional<HGCalMappingCellParamHostCollection> produce(const R& iRecord) {
        
        //get cell indexer
        HGCalMappingCellIndexer cpi = iRecord.get(cellIndexTkn_);

        const uint32_t size = cpi.getSize(); // channel-level size
        HGCalMappingCellParamHostCollection cellParams(size, cms::alpakatools::host());

        //open file and read the first line to identify which type it is
        edm::FileInPath fip(filename_);
        std::ifstream file(fip.fullPath());
        std::string line;
        std::getline(file, line);
        std::istringstream stream(line);
        std::string firststr;
        stream >> firststr;
        bool isSiPM = false;
        if(firststr == "index") isSiPM = true;
 
        //parse file and fill the SoA with the cell info
        bool isHD(false),iscalib(false);
        uint16_t type, chip, half,plane;
        uint16_t seq,rocpin(0);
        int sicell,triglink,trigcell,iu,iv,t,modiu;
        float trace;
        std::string typestr,denscol,rocpincol;
        while(std::getline(file, line))
          {
            std::istringstream stream(line);

            //SiPM version
            if(isSiPM) {
              stream >> seq >> plane >> iu >> iv >> typestr >> trigcell >> triglink >> modiu >> t;
              type = cpi.convertType(typestr);
            }

            //Si version
            else {
              stream >> denscol;
              isHD = denscol=="LD" ? false : true;
              stream >> type >> chip >> half >> seq;
              stream >> rocpincol;
              if(rocpincol.find("CALIB")!=std::string::npos) {
                iscalib=true;
                rocpin=uint16_t(rocpincol[rocpincol.size()-1]);
              }
              else {
                iscalib=false;
                rocpin=std::stoi(rocpincol);
              }
              stream >> sicell >> triglink >> trigcell >> iu >> iv >> trace >> t;
            }

            //get dense index and fill the values
            int i = cpi.denseIndex(type,chip,half,seq);
            cellParams.view()[i].isHD() = isHD;
            cellParams.view()[i].iscalib() = iscalib;
            cellParams.view()[i].type() = type;
            cellParams.view()[i].chip() = chip;
            cellParams.view()[i].half() = half;
            cellParams.view()[i].seq() = seq;
            cellParams.view()[i].rocpin() = rocpin;
            cellParams.view()[i].sicell() = sicell;
            cellParams.view()[i].triglink() = triglink;
            cellParams.view()[i].trigcell() = trigcell;
            cellParams.view()[i].iu() = iu;
            cellParams.view()[i].iv() = iv;
            cellParams.view()[i].t() = t;
            cellParams.view()[i].trace() = trace;
          }

        return cellParams;
      }  // end of produce()
      

    private:
      edm::ESGetToken<HGCalMappingCellIndexer,R> cellIndexTkn_;
      const std::string filename_;
    };

    typedef HGCalMappingCellESProducer<HGCalMappingSiCellIndexerRcd> HGCalMappingSiCellESProducer;
    typedef HGCalMappingCellESProducer<HGCalMappingSiPMCellIndexerRcd> HGCalMappingSiPMCellESProducer;
      
  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalMappingSiCellESProducer);
DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalMappingSiPMCellESProducer);
