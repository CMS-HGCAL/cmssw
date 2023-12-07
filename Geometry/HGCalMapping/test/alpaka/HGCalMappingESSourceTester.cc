#include <iomanip> // for std::setw
#include <future>
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/EDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToDevice.h"
#include "DataFormats/PortableTestObjects/interface/alpaka/TestDeviceCollection.h"

#include "CondFormats/DataRecord/interface/HGCalMappingModuleIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiCellIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiPMCellIndexerRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHostCollection.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDeviceCollection.h"

#include "DataFormats/ForwardDetId/interface/HGCSiliconDetId.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalMappingESSourceTester : public stream::EDProducer<> {
  public:
    explicit HGCalMappingESSourceTester(const edm::ParameterSet&);
    static void fillDescriptions(edm::ConfigurationDescriptions&);

    uint16_t getEcondErx(uint16_t chip, uint16_t half);
    uint32_t getElectronicsId(bool zside, uint16_t fedid, uint16_t captureblock, uint16_t econdidx, int cellchip, int cellhalf, int cellseq);
    uint32_t getSiDetId(bool zside, int moduleplane, int moduleu, int modulev, int modulethickness, int celliu, int celliv);
    uint32_t getSiPMDetId(bool zside, int moduleplane, int modulev, int celliu, int celliv);
    std::map<uint32_t,uint32_t> mapSiGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                      const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                      bool geo2ele);
    std::map<uint32_t,uint32_t> mapSiPMGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                        const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                        bool geo2ele);
    
  private:

    void produce(device::Event&, device::EventSetup const&) override;
    void beginRun(edm::Run const&, edm::EventSetup const&) override;

    edm::ESWatcher<HGCalMappingModuleIndexerRcd> cfgWatcher_;
    edm::ESGetToken<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd> moduleIndexTkn_;
    edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiCellIndexerRcd> siIndexTkn_;
    edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiPMCellIndexerRcd> sipmIndexTkn_;
    device::ESGetToken<hgcal::HGCalMappingModuleParamDeviceCollection, HGCalMappingModuleIndexerRcd> moduleTkn_;
    device::ESGetToken<hgcal::HGCalMappingCellParamDeviceCollection, HGCalMappingSiCellIndexerRcd> sicellTkn_;
    device::ESGetToken<hgcal::HGCalMappingCellParamDeviceCollection, HGCalMappingSiPMCellIndexerRcd> sipmcellTkn_;

    const device::EDPutToken<portabletest::TestDeviceCollection> testCollToken_;
  };

  //
  HGCalMappingESSourceTester::HGCalMappingESSourceTester(const edm::ParameterSet& iConfig)
    : moduleIndexTkn_(esConsumes<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd>()),
      siIndexTkn_(esConsumes<HGCalMappingCellIndexer,HGCalMappingSiCellIndexerRcd>()),
      sipmIndexTkn_(esConsumes<HGCalMappingCellIndexer,HGCalMappingSiPMCellIndexerRcd>()),
      moduleTkn_(esConsumes(edm::ESInputTag(""))),
      sicellTkn_(esConsumes(edm::ESInputTag(""))),
      sipmcellTkn_(esConsumes(edm::ESInputTag(""))),
      testCollToken_{produces()} {      
  }

  //
  void HGCalMappingESSourceTester::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup){
  }

  void HGCalMappingESSourceTester::produce(device::Event& iEvent, device::EventSetup const& iSetup) {

    //put a dummy collection to the event
    portabletest::TestDeviceCollection testColl{0,iEvent.queue()};
    iEvent.emplace(testCollToken_, std::move(testColl));
    
    // if the cfg didn't change there's nothing else to do
    if (!cfgWatcher_.check(iSetup)) return;

    //get indexers
    auto modulesIdx = iSetup.getData(moduleIndexTkn_);
    auto siIdx = iSetup.getData(siIndexTkn_);
    auto sipmIdx = iSetup.getData(sipmIndexTkn_);
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "Dense indexers retrieved for HGCAL";
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Module indexer]"
                                                    << "\n\t max FED=" << modulesIdx.idxParams_.maxFEDsPerEndcap
                                                    <<" max CB/FED=" << modulesIdx.idxParams_.sLinkCaptureBlockMax
                                                    <<" max ECON/CB=" << modulesIdx.idxParams_.captureBlockECONDMax
                                                    <<" max eRx/ECON=" << modulesIdx.idxParams_.econdERXMax
                                                    <<" max ch/eRx=" << modulesIdx.idxParams_.erxChannelMax      
                                                    << "\n\t Total size is=" << modulesIdx.getSize()
                                                    << " size at ROC is=" << modulesIdx.getSize(true);
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Si cell indexer]"
                                                    << "\n\t max types=" << siIdx.idxParams_.moduleTypeMax
                                                    << "\t max ROC / type=" << siIdx.idxParams_.cellChipMax
                                                    << "\t max half / ROC =" << siIdx.idxParams_.halfROCMax
                                                    << "\t max ch / half=" << siIdx.idxParams_.channelSeqMax
                                                    << "\n\t Total size is=" << siIdx.getSize();
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[SiPM-on-tile cell indexer]"
                                                    << "\n\t max types=" << sipmIdx.idxParams_.moduleTypeMax
                                                    << "\t max ROC / type=" << sipmIdx.idxParams_.cellChipMax
                                                    << "\t max half / ROC =" << sipmIdx.idxParams_.halfROCMax
                                                    << "\t max ch / half=" << sipmIdx.idxParams_.channelSeqMax
                                                    << "\n\t Total size is=" << sipmIdx.getSize();
    

    auto const& modules = iSetup.getData(moduleTkn_);
    for(int i=0; i<modules.view().metadata().size(); i++) {
        LogDebug("HGCalMappingModuleParameter")
        << "idx = "         << i << ", "
        << "zside = "    << modules.view()[i].zside()   << ", "
        << "isSiPM = "    << modules.view()[i].isSiPM()   << ", "
        << "plane = "  << modules.view()[i].plane() << ", "
        << "u = " << modules.view()[i].u() << ", "
        << "v = " << modules.view()[i].v() << ", "
        << "fedid = " << modules.view()[i].fedid() << ", "
        << "localfedid = " << modules.view()[i].localfedid() << ", "
        << "type = " << modules.view()[i].type() << ", "
        << "captureblock = " << modules.view()[i].captureblock() << ", "
        << "econdidx = " << modules.view()[i].econdidx() << ", "
        << "captureblockidx = " << modules.view()[i].captureblockidx() << std::endl;
     }

    auto const& sicells = iSetup.getData(sicellTkn_);
    for(int i=0; i<sicells.view().metadata().size(); i++) {
      LogDebug("HGCalMappingSiCellParameter")
          << "idx = "         << i << ", "
          << "isHD = "   << sicells.view()[i].isHD()  << ", "
          << "iscalib = "  << sicells.view()[i].iscalib() << ", "
          << "type = " << sicells.view()[i].type() << ", "
          << "chip = " << sicells.view()[i].chip() << ", "
          << "half = " << sicells.view()[i].half() << ", "
          << "seq = " << sicells.view()[i].seq() << ", "
          << "rocpin = " << sicells.view()[i].rocpin() << ", "
          << "cellidx = " << sicells.view()[i].cellidx() << ", "
          << "triglink = " << sicells.view()[i].triglink() << ", "
          << "trigcell = " << sicells.view()[i].trigcell() << ", "
          << "iu = " << sicells.view()[i].iu() << ", "
          << "iv = " << sicells.view()[i].iv() << ", "
          << "t = " << sicells.view()[i].t() << ", "
          << "trace = " << sicells.view()[i].trace() << std::endl;
     }

    auto const& sipmcells = iSetup.getData(sipmcellTkn_);
    for(int i=0; i<sipmcells.view().metadata().size(); i++) {
      LogDebug("HGCalMappingSiPMCellParameter")
        << "idx = "  << i << ", "
        << "isHD = "   << sipmcells.view()[i].isHD()  << ", "
        << "iscalib = "  << sipmcells.view()[i].iscalib() << ", "
        << "type = " << sipmcells.view()[i].type() << ", "
        << "chip = " << sipmcells.view()[i].chip() << ", "
        << "half = " << sipmcells.view()[i].half() << ", "
        << "seq = " << sipmcells.view()[i].seq() << ", "
        << "rocpin = " << sipmcells.view()[i].rocpin() << ", "
        << "cellidx = " << sipmcells.view()[i].cellidx() << ", "
        << "triglink = " << sipmcells.view()[i].triglink() << ", "
        << "trigcell = " << sipmcells.view()[i].trigcell() << ", "
        << "iu = " << sipmcells.view()[i].iu() << ", "
        << "iv = " << sipmcells.view()[i].iv() << ", "
        << "t = " << sipmcells.view()[i].t() << ", "
        << "trace = " << sipmcells.view()[i].trace() << std::endl;
    }

    std::map<uint32_t,uint32_t> sigeo2ele = HGCalMappingESSourceTester::mapSiGeoToElectronics(modules, sicells, true);
    std::map<uint32_t,uint32_t> siele2geo = HGCalMappingESSourceTester::mapSiGeoToElectronics(modules, sicells, false);

    std::map<uint32_t,uint32_t> sipmgeo2ele = HGCalMappingESSourceTester::mapSiPMGeoToElectronics(modules, sipmcells, true);
    std::map<uint32_t,uint32_t> sipmele2geo = HGCalMappingESSourceTester::mapSiPMGeoToElectronics(modules, sipmcells, false);

    edm::LogInfo("HGCalMappingIndexESSourceTester") << "Module and cells maps retrieved for HGCAL";
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Silicon cell map]"
                                                    << "\n\t ID maps ele2geo=" << siele2geo.size()
                                                    <<" ID maps geo2ele=" << sigeo2ele.size() << std::endl;
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[SiPM-on-tile cell map]"
                                                    << "\n\t ID maps ele2geo=" << sipmele2geo.size()
                                                    <<" ID maps geo2ele=" << sipmgeo2ele.size() << std::endl;
    
    assert(sigeo2ele.size()==siele2geo.size());
    assert(sipmgeo2ele.size()==sipmele2geo.size());

    for(auto it : sigeo2ele) {
      assert(siele2geo.count(it.second)==1);
      assert(siele2geo[it.second]==it.first);
    }
    for(auto it : sipmgeo2ele) {
      assert(sipmele2geo.count(it.second)==1);
      assert(sipmele2geo[it.second]==it.first);
    }

    for(auto it : siele2geo) {
      assert(sigeo2ele.count(it.second)==1);
      assert(sigeo2ele[it.second]==it.first);
    }
    for(auto it : sipmele2geo) {
      assert(sipmgeo2ele.count(it.second)==1);
      assert(sipmgeo2ele[it.second]==it.first);
    }

  }

  //
  void HGCalMappingESSourceTester::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    descriptions.addWithDefaultLabel(desc);
  }

  uint16_t HGCalMappingESSourceTester::getEcondErx(uint16_t chip, uint16_t half){
    return chip*2+half;
  }

  uint32_t HGCalMappingESSourceTester::getElectronicsId(bool zside, uint16_t fedid, uint16_t captureblock, uint16_t econdidx, int cellchip, int cellhalf, int cellseq) {
    uint16_t econderx = HGCalMappingESSourceTester::getEcondErx(cellchip, cellhalf);

    return HGCalElectronicsId(zside,fedid,captureblock,econdidx,econderx,cellseq).raw();
  }

  uint32_t HGCalMappingESSourceTester::getSiDetId(bool zside, int moduleplane, int moduleu, int modulev, int modulethickness, int celliu, int celliv) {
    
    DetId::Detector det = moduleplane<=26 ? DetId::Detector::HGCalEE : DetId::Detector::HGCalHSi;
    int zp(zside ?  1 : -1);

    return HGCSiliconDetId(det,zp,modulethickness,moduleplane,moduleu,modulev,celliu,celliv).rawId();
  }

  uint32_t HGCalMappingESSourceTester::getSiPMDetId(bool zside, int moduleplane, int modulev, int celliu, int celliv) {
    int layer = moduleplane - 25;
    int type = 1; // 4mm2
    int sipm = 1; // c
    if(moduleplane <= 37) type = 2; // 9mm2
    else if(moduleplane >= 38 && moduleplane <= 39 && celliu <=17) type = 2;
    else if(moduleplane >= 40 && celliu <= 9) type = 2;
    else if(moduleplane >= 42 && celliu >=18) sipm = 2; // m

    int ring = (zside ? celliu : (-1)*celliu);
    int iphi = modulev*8 + celliv + 1;

    return HGCScintillatorDetId(type, layer, ring, iphi, false, sipm).rawId();
  }

  std::map<uint32_t,uint32_t> HGCalMappingESSourceTester::mapSiGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                                                const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                                                bool geo2ele)
  {
    //loop over SiPM tileboards
    std::map<uint32_t,uint32_t> idmap;
    for(int i=0; i<modules.view().metadata().size(); i++) {
      
      if(modules.view()[i].isSiPM()) continue;

      //loop over tiles in this tileboard
      for(int j=0; j<cells.view().metadata().size(); j++) {
        
        // make sure the cell is part of the module
        if(cells.view()[j].t()!=1) continue;
        if(cells.view()[j].isHD()!=modules.view()[i].isHD()) continue;
        if(cells.view()[j].type()!=modules.view()[i].type()) continue;

        uint32_t elecid = HGCalMappingESSourceTester::getElectronicsId(modules.view()[i].zside(),
                                                                      modules.view()[i].fedid(),
                                                                      modules.view()[i].captureblock(),
                                                                      modules.view()[i].econdidx(),
                                                                      cells.view()[j].chip(),
                                                                      cells.view()[j].half(),
                                                                      cells.view()[j].seq());

        uint32_t geoid = HGCalMappingESSourceTester::getSiDetId(modules.view()[i].zside(), 
                                                                modules.view()[i].plane(), 
                                                                modules.view()[i].u(),
                                                                modules.view()[i].v(), 
                                                                modules.view()[i].thickness(),
                                                                cells.view()[j].iu(), 
                                                                cells.view()[j].iv());

        if(geo2ele){
          auto it = idmap.find(geoid);
          if(it != idmap.end()){
            std::cout << "Geo ID already in map!" << std::endl;
          }
        }
        if(!geo2ele){
          auto it = idmap.find(elecid);
          if(it != idmap.end()){
            std::cout << "Elec ID already in map!" << std::endl;
          }
        }
        //map
        idmap[geo2ele ? geoid : elecid] = geo2ele ? elecid : geoid;
      }
    }

    return idmap;
  }

  std::map<uint32_t,uint32_t> HGCalMappingESSourceTester::mapSiPMGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                                                  const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                                                  bool geo2ele)
  {
    //loop over SiPM tileboards
    std::map<uint32_t,uint32_t> idmap;
    for(int i=0; i<modules.view().metadata().size(); i++) {
      
      if(!modules.view()[i].isSiPM()) continue;

      //loop over tiles in this tileboard
      for(int j=0; j<cells.view().metadata().size(); j++) {
        
        // make sure the tile is part of the tileboard
        if(cells.view()[j].t()==-1) continue;
        if(cells.view()[j].type()!=modules.view()[i].type()) continue;

        uint32_t elecid = HGCalMappingESSourceTester::getElectronicsId(modules.view()[i].zside(),
                                                                      modules.view()[i].fedid(),
                                                                      modules.view()[i].captureblock(),
                                                                      modules.view()[i].econdidx(),
                                                                      cells.view()[j].chip(),
                                                                      cells.view()[j].half(),
                                                                      cells.view()[j].seq());

        uint32_t geoid = HGCalMappingESSourceTester::getSiPMDetId(modules.view()[i].zside(), 
                                                                  modules.view()[i].plane(), 
                                                                  modules.view()[i].v(), 
                                                                  cells.view()[j].iu(), 
                                                                  cells.view()[j].iv());

        if(geo2ele){
          auto it = idmap.find(geoid);
          if(it != idmap.end()){
            std::cout << "Geo ID already in map!" << std::endl;
          }
        }
        if(!geo2ele){
          auto it = idmap.find(elecid);
          if(it != idmap.end()){
            std::cout << "Elec ID already in map!" << std::endl;
          }
        }
        //map
        idmap[geo2ele ? geoid : elecid] = geo2ele ? elecid : geoid;
      }
    }

    return idmap;
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

// define this as a plug-in
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalMappingESSourceTester);
