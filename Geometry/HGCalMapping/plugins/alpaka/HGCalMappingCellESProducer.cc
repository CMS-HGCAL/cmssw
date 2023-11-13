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

#include "Geometry/HGCalMapping/interface/HGCalMappingCellParameterIndex.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingParameterHostCollection.h"
#include "Geometry/HGCalMapping/interface/alpaka/HGCalMappingParameterDeviceCollection.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcal {

    class HGCalMappingCellESProducer : public ESProducer {
    public:

      HGCalMappingCellESProducer(const edm::ParameterSet& iConfig)
        : ESProducer(iConfig),
          filename_(iConfig.getParameter<std::string>("filename")) {
      }

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<std::string>("filename", {});
        descriptions.addWithDefaultLabel(desc);
      }

      std::optional<hgcal::HGCalMappingCellParamHostCollection> produce(const HGCalCondSerializableModuleInfoRcd& iRecord) {

        // load dense indexing
        HGCalMappingCellParameterIndex cpi;
        const uint32_t size = cpi.getSize(); // channel-level size
        hgcal::HGCalMappingCellParamHostCollection cellParams(size, cms::alpakatools::host());
        cellParams.view().config() = cpi; // set dense indexing in SoA

        // load si cell mapping parameters
        edm::FileInPath fip(filename_);
        std::ifstream file(fip.fullPath());
        std::string line;
        // size_t iline(0);
        int i = 0;

        std::getline(file, line);
        std::istringstream stream(line);
        std::string firststr;
        stream >> firststr;
        bool isSiPM = false;
        if(firststr == "index") isSiPM = true;

        if(isSiPM){

          int seq,plane,iu,iv,trigcell,triglink,modiu,t,type;
          std::string typestr;

          while(std::getline(file, line))
          {
            // iline++;
            // if(iline==1) continue;
            std::istringstream stream(line);

            stream >> seq >> plane >> iu >> iv >> typestr >> trigcell >> triglink >> modiu >> t;
            type = cpi.convertType(typestr);

            cellParams.view()[i].seq() = seq;
            // cellParams.view()[i].plane() = plane;
            cellParams.view()[i].iu() = iu;
            cellParams.view()[i].iv() = iv;
            cellParams.view()[i].trigcell() = trigcell;
            cellParams.view()[i].triglink() = triglink;
            cellParams.view()[i].modiu() = modiu;
            cellParams.view()[i].t() = t;
            cellParams.view()[i].type() = type;
            i++;
          }  

        } else { 

          bool isHD,iscalib;
          uint16_t type, chip, half;
          uint16_t seq,rocpin;
          int sicell,triglink,trigcell,iu,iv,t;
          float trace;

          while(std::getline(file, line))
          {
            // iline++;
            // if(iline==1) continue;
            std::istringstream stream(line);

            std::string denscol,rocpincol;
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

            i++;
          }

        }

        return cellParams;
      }  // end of produce()

    private:
      const std::string filename_;
    };

  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalMappingCellESProducer);