#include <yaml-cpp/yaml.h>

#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/SourceFactory.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESProducer.h"
#include "FWCore/Framework/interface/EventSetupRecordIntervalFinder.h"
#include "FWCore/Framework/interface/ESProducts.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiCellChannelInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiPMTileInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiPMTileInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalModuleLocator.h"
#include "Geometry/HGCalMapping/interface/HGCalSiCellLocator.h"
#include "Geometry/HGCalMapping/interface/HGCalSiPMCellLocator.h"

/**
   @short 
   takes care of steering the parsing of the electronics mapping files and creating a ESSource
   the class is templated as the methods of filling the serializable objects are delegated to the 
   methods under Geometry/HGCalMapping
 */
template <class R, class T, class H>
class HGCalElectronicsMapESSourceBase : public edm::ESProducer, public edm::EventSetupRecordIntervalFinder {
public:
  explicit HGCalElectronicsMapESSourceBase(const edm::ParameterSet& iConfig)
      : filename_(iConfig.getParameter<std::string>("filename")) {
    setWhatProduced(this);
    findingRecord<R>();
  }

  std::unique_ptr<T> produce(const R&) { return parseFromFile(filename_); }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<std::string>("filename", {});
    descriptions.addWithDefaultLabel(desc);
  }

private:

  /**
     @short defines the validity of the IOV
   */
  void setIntervalFor(const edm::eventsetup::EventSetupRecordKey&,
                      const edm::IOVSyncValue&,
                      edm::ValidityInterval& oValidity) override {
    oValidity = edm::ValidityInterval(edm::IOVSyncValue::beginOfTime(), edm::IOVSyncValue::endOfTime());
  }


  /**
     @short steers the parsing and moves the info to a unique_ptr to put in the event setup
   */
  std::unique_ptr<T> parseFromFile(const std::string& filename) {
    H filler;
    filler.buildLocatorFrom(filename,true);
    auto cond = std::make_unique<T>(filler.getInfo());
    return cond;
  }

  const std::string filename_;
};


//specify the different ES sources
typedef HGCalElectronicsMapESSourceBase<HGCalCondSerializableModuleInfoRcd,HGCalCondSerializableModuleInfo,HGCalModuleLocator> HGCalModuleInfoESSource;
typedef HGCalElectronicsMapESSourceBase<HGCalCondSerializableSiCellChannelInfoRcd,HGCalCondSerializableSiCellChannelInfo,HGCalSiCellLocator> HGCalSiModuleInfoESSource;
typedef HGCalElectronicsMapESSourceBase<HGCalCondSerializableSiPMTileInfoRcd,HGCalCondSerializableSiPMTileInfo,HGCalSiPMCellLocator> HGCalSiPMModuleInfoESSource;

DEFINE_FWK_EVENTSETUP_SOURCE(HGCalModuleInfoESSource);
DEFINE_FWK_EVENTSETUP_SOURCE(HGCalSiModuleInfoESSource);
DEFINE_FWK_EVENTSETUP_SOURCE(HGCalSiPMModuleInfoESSource);
