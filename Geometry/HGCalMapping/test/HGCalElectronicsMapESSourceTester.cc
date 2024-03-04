#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiCellChannelInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiPMTileInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiPMTileInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalElectronicsMappingTools.h"

#include <iostream>

/**
   @short a tester for the logical mapping as ESsource
 */
class HGCalElectronicsMapESSourceTester : public edm::one::EDAnalyzer<edm::one::WatchRuns> {

public:

  /**
     @short constructor
   */
  explicit HGCalElectronicsMapESSourceTester(const edm::ParameterSet& iConfig)
    :  moduleInfoToken_(esConsumes<HGCalCondSerializableModuleInfo,HGCalCondSerializableModuleInfoRcd,edm::Transition::BeginRun>(iConfig.getParameter<edm::ESInputTag>("ModuleInfo"))),
       siModuleInfoToken_(esConsumes<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd,edm::Transition::BeginRun>(iConfig.getParameter<edm::ESInputTag>("SiModuleInfo"))),
       sipmModuleInfoToken_(esConsumes<HGCalCondSerializableSiPMTileInfo,HGCalCondSerializableSiPMTileInfoRcd,edm::Transition::BeginRun>(iConfig.getParameter<edm::ESInputTag>("SiPMModuleInfo")))
  {}

  /**
     @short description filler
   */
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::ESInputTag>("ModuleInfo",edm::ESInputTag(""));
    desc.add<edm::ESInputTag>("SiModuleInfo",edm::ESInputTag(""));
    desc.add<edm::ESInputTag>("SiPMModuleInfo",edm::ESInputTag(""));
    descriptions.addWithDefaultLabel(desc);
  }
  
private:

  /**
     @short check what has been put in the event setup at start of the run
   */
  void beginRun(edm::Run const& , edm::EventSetup const&) override;

  /**
     @short do something event-by-event
   */
  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {}

  /**
     @short do another thing end of the run
  */
  void endRun(edm::Run const& , edm::EventSetup const&) override {}

  /**
     @short do something end of the job
  */
  void endJob() override {}
  
  //tokens and record watches
  edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
  edm::ESGetToken<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd> siModuleInfoToken_;
  edm::ESGetToken<HGCalCondSerializableSiPMTileInfo,HGCalCondSerializableSiPMTileInfoRcd> sipmModuleInfoToken_;
};

//
void HGCalElectronicsMapESSourceTester::beginRun(edm::Run const& iRun, const edm::EventSetup& iSetup) {

  auto moduleInfo = iSetup.getData(moduleInfoToken_);
  std::tuple<uint16_t,uint8_t,uint8_t,uint8_t> denseIdxMax = moduleInfo.getMaxValuesForDenseIndex();
  HGCalCondSerializableModuleInfo::ERxBitPatternMap erxbitmap = moduleInfo.getERxBitPattern();

  auto siCellInfo = iSetup.getData(siModuleInfoToken_);
  std::map<uint32_t,uint32_t> ele2geo=hgcal::mapSiGeoToElectronics(moduleInfo,siCellInfo,false);
  std::map<uint32_t,uint32_t> geo2ele=hgcal::mapSiGeoToElectronics(moduleInfo,siCellInfo,true);

  auto sipmCellInfo = iSetup.getData(sipmModuleInfoToken_);
  std::map<uint32_t,uint32_t> sipmEle2geo=hgcal::mapSiPMGeoToElectronics(moduleInfo,sipmCellInfo,false);
  std::map<uint32_t,uint32_t> sipmGeo2ele=hgcal::mapSiPMGeoToElectronics(moduleInfo,sipmCellInfo,true);

  std::cout << "Read module info with " << moduleInfo.params_.size() << " entries" << std::endl
            << "Max values for dense indexing are" << std::endl
            << "\tmax fed=" << (uint32_t)(std::get<0>(denseIdxMax)) << std::endl
            << "\tmax capture=" << (uint32_t)(std::get<1>(denseIdxMax)) << std::endl
            << "\tmax econd=" << (uint32_t)(std::get<2>(denseIdxMax)) << std::endl
            << "\tmax eRx=" << (uint32_t)(std::get<3>(denseIdxMax)) << std::endl
            << "Read si cell info with " << siCellInfo.params_.size() << " entries" << std::endl
            << "si ID maps #ele2geo=" << ele2geo.size() << " #geo2ele=" << geo2ele.size() << std::endl
            << "Read SiPM cell info with " << sipmCellInfo.params_.size() << " entries" << std::endl
            << "SiPM ID maps #ele2geo=" << sipmEle2geo.size() << " #geo2ele=" << sipmGeo2ele.size() << std::endl;
  std::cout << "e-Rx enable bit patterns" << std::endl;
  for(auto it : erxbitmap){
    std::cout << "\t" << it.first << " : " << it.second << std::endl;
  }
  
  // si cell test
  assert(ele2geo.size()==geo2ele.size());

  for(auto it : ele2geo) {
    assert(geo2ele.count(it.second)==1);
    assert(geo2ele[it.second]==it.first);
  }
  
  for(auto it : geo2ele) {
    assert(ele2geo.count(it.second)==1);
    assert(ele2geo[it.second]==it.first);
  }

  std::cout << "1:1 correspondences found for physical si cells" << std::endl;

  // sipm cell test
  assert(sipmEle2geo.size()==sipmGeo2ele.size());

  for(auto it : sipmEle2geo) {
    assert(sipmGeo2ele.count(it.second)==1);
    assert(sipmGeo2ele[it.second]==it.first);
  }
  
  for(auto it : sipmGeo2ele) {
    assert(sipmEle2geo.count(it.second)==1);
    assert(sipmEle2geo[it.second]==it.first);
  }

  std::cout << "1:1 correspondences found for physical SiPM cells" << std::endl;
}


DEFINE_FWK_MODULE(HGCalElectronicsMapESSourceTester);
