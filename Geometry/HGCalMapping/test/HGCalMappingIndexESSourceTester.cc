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

      //print module info
      edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Module indexer]"
                                                      << "\n\t max FED=" << modules.idxParams_.maxFEDsPerEndcap
                                                      <<" max CB/FED=" << modules.idxParams_.sLinkCaptureBlockMax
                                                      <<" max ECON/CB=" << modules.idxParams_.captureBlockECONDMax
                                                      <<" max eRx/ECON=" << modules.idxParams_.econdERXMax
                                                      <<" max ch/eRx=" << modules.idxParams_.erxChannelMax      
                                                      << "\n\t Total size is=" << modules.getSize()
                                                      << " size at ROC is=" << modules.getSize(true);

      //print Si Cell info
      edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Si cell indexer]"
                                                      << "\n\t max types=" << si.idxParams_.moduleTypeMax
                                                      << "\t max ROC / type=" << si.idxParams_.cellChipMax
                                                      << "\t max half / ROC =" << si.idxParams_.halfROCMax
                                                      << "\t max ch / half=" << si.idxParams_.channelSeqMax
                                                      << "\n\t Total size is=" << si.getSize();

      //print SiPM-on-tile Cell info
      edm::LogInfo("HGCalMappingIndexESSourceTester") << "[SiPM-on-tile cell indexer]"
                                                      << "\n\t max types=" << sipm.idxParams_.moduleTypeMax
                                                      << "\t max ROC / type=" << sipm.idxParams_.cellChipMax
                                                      << "\t max half / ROC =" << sipm.idxParams_.halfROCMax
                                                      << "\t max ch / half=" << sipm.idxParams_.channelSeqMax
                                                      << "\n\t Total size is=" << sipm.getSize();
    }
    
  }

  edm::ESWatcher<HGCalMappingModuleIndexerRcd> cfgWatcher_;
  edm::ESGetToken<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd> moduleIndexTkn_;
  edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiCellIndexerRcd> siIndexTkn_;
  edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiPMCellIndexerRcd> sipmIndexTkn_;
};

DEFINE_FWK_MODULE(HGCalMappingIndexESSourceTester);
