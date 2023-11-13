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

// #include "CondFormats/HGCalObjects/interface/HGCalCondSerializableDenseIndex.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcal {

    class HGCalMappingCellIndexESProducer : public ESProducer {
    public:

      HGCalMappingCellIndexESProducer(const edm::ParameterSet& iConfig)
        : ESProducer(iConfig),
          filename_(iConfig.getParameter<std::string>("filename")) {
      }

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<std::string>("filename", {});
        descriptions.addWithDefaultLabel(desc);
      }

      std::optional<HGCalMappingCellParameterIndex> produce(const HGCalCondSerializableModuleInfoRcd& iRecord) {

        HGCalMappingCellParameterIndex cpi;

        // load module mapping parameters
        edm::FileInPath fip(filename_);
        std::ifstream file(fip.fullPath());
        std::string line;
        // size_t iline(0);
        uint16_t maxtype(0),maxchip(0),maxhalf(0),maxseq(0);

        std::getline(file, line);
        std::istringstream stream(line);
        std::string firststr;
        stream >> firststr;
        bool isSiPM = false;
        if(firststr == "index") isSiPM = true;

        if(isSiPM){

          int plane,iu,iv,trigcell,triglink,modiu,t;
          uint16_t type, seq;
          std::string typestr;

          while(std::getline(file, line))
          {
            // iline++;
            // if(iline==1) continue;
            std::istringstream stream(line);

            stream >> seq >> plane >> iu >> iv >> typestr >> trigcell >> triglink >> modiu >> t;
            type = cpi.convertType(typestr);

            maxtype=std::max(type,maxtype);
            maxhalf=2;
            maxseq=std::max(seq,maxseq);
          }

        } else { 

          uint16_t type, chip, half;
          uint16_t seq;
          int sicell,triglink,trigcell,iu,iv,t;
          float trace;

          while(std::getline(file, line))
          {
            // iline++;
            // if(iline==1) continue;
            std::istringstream stream(line);

            std::string denscol,rocpincol;
            stream >> denscol;
            stream >> type >> chip >> half >> seq;
            stream >> rocpincol;
            stream >> sicell >> triglink >> trigcell >> iu >> iv >> trace >> t;

            maxtype=std::max(type,maxtype);
            maxchip=std::max(chip,maxchip);
            maxhalf=std::max(half,maxhalf);
            maxseq=std::max(seq,maxseq);
          }
        }

        cpi.setMaxValues(maxtype,maxchip,maxhalf,maxseq);

        return cpi;
      }  // end of produce()

    private:
      const std::string filename_;
    };

  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalMappingCellIndexESProducer);