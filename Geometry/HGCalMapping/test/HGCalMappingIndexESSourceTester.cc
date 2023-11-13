#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "CondFormats/DataRecord/interface/HGCalMappingModuleIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiCellIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiPMCellIndexerRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"

class HGCalMappingIndexESSourceTester : public edm::one::EDAnalyzer<> {

public:
  
  explicit HGCalMappingIndexESSourceTester(const edm::ParameterSet& iConfig)
    : moduleIndexTkn_(esConsumes<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd>()),
      siIndexTkn_(esConsumes<HGCalMappingCellIndexer,HGCalMappingSiCellIndexerRcd>()),
      sipmIndexTkn_(esConsumes<HGCalMappingCellIndexer,HGCalMappingSiPMCellIndexerRcd>()) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    descriptions.addWithDefaultLabel(desc);
  }

private:

  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {

    // get timing calibration parameters
    if (cfgWatcher_.check(iSetup)) {
      auto modules = iSetup.getData(moduleIndexTkn_);
      auto si = iSetup.getData(siIndexTkn_);
      auto sipm = iSetup.getData(sipmIndexTkn_);
      edm::LogInfo("HGCalMappingIndexESSourceTester") << "Dense indexers retrieved for HGCAL";
    }

  }

  edm::ESWatcher<HGCalMappingModuleIndexerRcd> cfgWatcher_;
  edm::ESGetToken<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd> moduleIndexTkn_;
  edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiCellIndexerRcd> siIndexTkn_;
  edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiPMCellIndexerRcd> sipmIndexTkn_;
};

DEFINE_FWK_MODULE(HGCalMappingIndexESSourceTester);
