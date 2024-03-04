#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/SourceFactory.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESProducer.h"
#include "FWCore/Framework/interface/EventSetupRecordIntervalFinder.h"
#include "FWCore/Framework/interface/ESProducts.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "DataFormats/Math/interface/libminifloat.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializablePedestalsRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializablePedestals.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

/**
   @short simple plugin to parse a pedestals file and construct the pedestals object
 */
class HGCalPedestalsESSource : public edm::ESProducer, public edm::EventSetupRecordIntervalFinder {
public:
  explicit HGCalPedestalsESSource(const edm::ParameterSet& iConfig)
      : filename_(iConfig.getParameter<std::string>("filename")) {
    setWhatProduced(this);
    findingRecord<HGCalCondSerializablePedestalsRcd>();
  }

  std::unique_ptr<HGCalCondSerializablePedestals> produce(const HGCalCondSerializablePedestalsRcd&) { return parsePedestals(filename_); }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<std::string>("filename", {});
    descriptions.addWithDefaultLabel(desc);
  }

private:
  void setIntervalFor(const edm::eventsetup::EventSetupRecordKey&,
                      const edm::IOVSyncValue&,
                      edm::ValidityInterval& oValidity) override {
    oValidity = edm::ValidityInterval(edm::IOVSyncValue::beginOfTime(), edm::IOVSyncValue::endOfTime());
  }

  std::unique_ptr<HGCalCondSerializablePedestals> parsePedestals(const std::string& filename);
  
  const std::string filename_;
};


//
std::unique_ptr<HGCalCondSerializablePedestals> HGCalPedestalsESSource::parsePedestals(const std::string& filename)
{
    auto cond = std::make_unique<HGCalCondSerializablePedestals>();

    //open file
    edm::FileInPath fip(filename);
    std::ifstream file(fip.fullPath());
    std::string line;
    uint32_t id;
    float ped,cm_slope,cm_off,kappa_bxm1;
    while(std::getline(file, line))
      {
        if(line.find("Channel")!=std::string::npos || line.find("#")!=std::string::npos) continue;

        std::istringstream stream(line);
        stream >> id >> ped >> cm_slope >> cm_off >> kappa_bxm1;

        //reduce to half-point float and fill the pedestals of this channel
        HGCalPedestals m;        
        m.pedestal = MiniFloatConverter::float32to16(ped);
        m.cm_slope = MiniFloatConverter::float32to16(cm_slope);
        m.cm_offset = MiniFloatConverter::float32to16(cm_off);
        m.kappa_bxm1 = MiniFloatConverter::float32to16(kappa_bxm1);
        cond->addParameter(id,m);
      }

    //return the conditions
    return cond;
  }


DEFINE_FWK_EVENTSETUP_SOURCE(HGCalPedestalsESSource);
