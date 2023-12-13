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
#include "CondFormats/DataRecord/interface/HGCalMappingCellIndexerRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDeviceCollection.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingTools.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalMappingESSourceTester : public stream::EDProducer<> {
  public:
    explicit HGCalMappingESSourceTester(const edm::ParameterSet&);
    static void fillDescriptions(edm::ConfigurationDescriptions&);
    /*
    std::map<uint32_t,uint32_t> mapSiGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                      const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                      bool geo2ele);
    std::map<uint32_t,uint32_t> mapSiPMGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                        const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                        bool geo2ele);
    */
  private:

    void produce(device::Event&, device::EventSetup const&) override;
    void beginRun(edm::Run const&, edm::EventSetup const&) override;

    edm::ESWatcher<HGCalMappingModuleIndexerRcd> cfgWatcher_;
    //edm::ESGetToken<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd> moduleIndexTkn_;
    edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingCellIndexerRcd> cellIndexTkn_;
    //device::ESGetToken<hgcal::HGCalMappingModuleParamDeviceCollection, HGCalMappingModuleIndexerRcd> moduleTkn_;
    device::ESGetToken<hgcal::HGCalMappingCellParamDeviceCollection, HGCalMappingCellIndexerRcd> cellTkn_;
    const device::EDPutToken<portabletest::TestDeviceCollection> testCollToken_;
  };

  //
  HGCalMappingESSourceTester::HGCalMappingESSourceTester(const edm::ParameterSet& iConfig)
    : //moduleIndexTkn_(esConsumes<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd>()),
      cellIndexTkn_(esConsumes<HGCalMappingCellIndexer,HGCalMappingCellIndexerRcd>()),      
      //moduleTkn_(esConsumes(edm::ESInputTag(""))),
      cellTkn_(esConsumes(edm::ESInputTag(""))),      
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
    //auto modulesIdx = iSetup.getData(moduleIndexTkn_);
    auto cellIdx = iSetup.getData(cellIndexTkn_);
    //auto sipmIdx = iSetup.getData(sipmIndexTkn_);
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "Dense indexers retrieved for HGCAL";
    /*
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Module indexer]"
                                                    << "\n\t max FED=" << modulesIdx.idxParams_.maxFEDsPerEndcap
                                                    <<" max CB/FED=" << modulesIdx.idxParams_.sLinkCaptureBlockMax
                                                    <<" max ECON/CB=" << modulesIdx.idxParams_.captureBlockECONDMax
                                                    <<" max eRx/ECON=" << modulesIdx.idxParams_.econdERXMax
                                                    <<" max ch/eRx=" << modulesIdx.idxParams_.erxChannelMax      
                                                    << "\n\t Total size is=" << modulesIdx.getSize()
                                                    << " size at ROC is=" << modulesIdx.getSize(true);
    */

    //printout the contents
    size_t nmodules=cellIdx.typeCodeIndexer_.size();
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Module cell indexer] has " << nmodules << " module types" << std::endl;
    edm::LogInfo("HGCalMappingIndexESSourceTester").log( [&](auto& log) {

      uint32_t totOffset(0);
      for(size_t idx=0; idx<cellIdx.di_.size(); idx++) {

        //check typecode exists
        auto typecode = cellIdx.getTypecodeFromEnum(idx);
        assert( cellIdx.typeCodeIndexer_.count(typecode)==1);        

        //check that the current offset is consistent with the increment from cells from the previous module        
        if(idx>0) {
          uint32_t nch_prev = cellIdx.maxErx_[idx-1]*cellIdx.maxChPerErx_;
          uint32_t delta_offset = cellIdx.offsets_[idx]-cellIdx.offsets_[idx-1];
          assert(delta_offset==nch_prev);
        }

        //assert offset is consistent with the accumulation        
        auto off = cellIdx.offsets_[idx];
        assert(off==totOffset);

        totOffset += cellIdx.maxErx_[idx]*cellIdx.maxChPerErx_;
        
        //print
        log << "\t [" << typecode << "] has "
            << " index(internal)=" << idx
            << " #eRx = " << cellIdx.maxErx_[idx]
            << " #cells = " << cellIdx.di_[idx].getMaxIndex()
            << " offset @ " << cellIdx.offsets_[idx] << "\n";
      }
            
      assert(totOffset == cellIdx.maxDenseIndex() );
      log << "SoA size for module cell mapping will be " << totOffset << "\n";
    });

    //Module cells SoA contents
    auto const& cells = iSetup.getData(cellTkn_);
    uint32_t ncells=cells.view().metadata().size();
    assert(ncells==cellIdx.maxDenseIndex()); //check for consistent size
    LogDebug("HGCalMappingIndexESSourceTester").log( [&](auto& log) {
      log << "Module cell mapping contents \n";
      for(uint32_t i=0; i<ncells; i++) {
        log << "\tidx = "         << i << ", "
            << "isHD = "   << cells.view()[i].isHD()  << ", "
            << "iscalib = "  << cells.view()[i].iscalib() << ", "
            << "isSiPM = "  << cells.view()[i].isSiPM() << ", "
            << "type = " << cells.view()[i].type() << ", "
            << "chip = " << cells.view()[i].chip() << ", "
            << "half = " << cells.view()[i].half() << ", "
            << "seq = " << cells.view()[i].seq() << ", "
            << "rocpin = " << cells.view()[i].rocpin() << ", "
            << "cellidx = " << cells.view()[i].cellidx() << ", "
            << "triglink = " << cells.view()[i].triglink() << ", "
            << "trigcell = " << cells.view()[i].trigcell() << ", "
            << "iu = " << cells.view()[i].iu() << ", "
            << "iv = " << cells.view()[i].iv() << ", "
            << "t = " << cells.view()[i].t() << ", "
            << "trace = " << cells.view()[i].trace() << ", "
            << "thickness = " << cells.view()[i].thickness() << "\n";
      }
    });


    /*
    auto const& modules = iSetup.getData(moduleTkn_);
    for(int i=0; i<modules.view().metadata().size(); i++) {
        LogDebug("HGCalMappingModuleParameter")
        << "idx = "         << i << ", "
        << "zside = "    << modules.view()[i].zside()   << ", "
        << "isSiPM = "    << modules.view()[i].isSiPM()   << ", "
        << "isSiPM = "    << modules.view()[i].isHD()   << ", "
        << "plane = "  << modules.view()[i].plane() << ", "
        << "u = " << modules.view()[i].u() << ", "
        << "v = " << modules.view()[i].v() << ", "
        << "v = " << modules.view()[i].thickness() << ", "
        << "type = " << modules.view()[i].type() << ", "
        << "fedid = " << modules.view()[i].fedid() << ", "
        << "localfedid = " << modules.view()[i].slinkidx() << ", "
        << "captureblock = " << modules.view()[i].captureblock() << ", "
        << "captureblockidx = " << modules.view()[i].captureblockidx() << ", "
        << "econdidx = " << modules.view()[i].econdidx() << ", " << std::endl;
     }

    std::map<uint32_t,uint32_t> sigeo2ele = this->mapSiGeoToElectronics(modules, sicells, true);
    std::map<uint32_t,uint32_t> siele2geo = this->mapSiGeoToElectronics(modules, sicells, false);

    std::map<uint32_t,uint32_t> sipmgeo2ele = this->mapSiPMGeoToElectronics(modules, sipmcells, true);
    std::map<uint32_t,uint32_t> sipmele2geo = this->mapSiPMGeoToElectronics(modules, sipmcells, false);

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

    */
  }

  //
  void HGCalMappingESSourceTester::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    descriptions.addWithDefaultLabel(desc);
  }
  
  //
  /*
  std::map<uint32_t,uint32_t> HGCalMappingESSourceTester::mapSiGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                                                const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                                                bool geo2ele)
  {
    //loop over SiPM tileboards
    std::map<uint32_t,uint32_t> idmap;
    uint32_t ndups(0);
    for(int i=0; i<modules.view().metadata().size(); i++) {
      
      if(modules.view()[i].isSiPM()) continue;

      //loop over tiles in this tileboard
      for(int j=0; j<cells.view().metadata().size(); j++) {
        
        // make sure the cell is part of the module
        if(cells.view()[j].t()!=1) continue;
        if(cells.view()[j].isHD()!=modules.view()[i].isHD()) continue;
        if(cells.view()[j].type()!=modules.view()[i].type()) continue;

        uint32_t elecid = hgcal::mappingtools::getElectronicsId(modules.view()[i].zside(),
                                                                modules.view()[i].fedid(),
                                                                modules.view()[i].captureblockidx(),
                                                                modules.view()[i].econdidx(),
                                                                cells.view()[j].chip(),
                                                                cells.view()[j].half(),
                                                                cells.view()[j].seq());
        
        uint32_t geoid = hgcal::mappingtools::getSiDetId(modules.view()[i].zside(), 
                                                         modules.view()[i].plane(), 
                                                         modules.view()[i].u(),
                                                         modules.view()[i].v(), 
                                                         modules.view()[i].thickness(),
                                                         cells.view()[j].iu(), 
                                                         cells.view()[j].iv());
        
        if(geo2ele){
          auto it = idmap.find(geoid);
          ndups += (it != idmap.end());
        }
        if(!geo2ele){
          auto it = idmap.find(elecid);
          ndups += (it != idmap.end());
        }
        
        //map
        idmap[geo2ele ? geoid : elecid] = geo2ele ? elecid : geoid;
      }
    }

    if(ndups>0) {
      edm::LogInfo("HGCalMappingIndexESSourceTester") << "mapSiGeoToElectronics found " << ndups << " duplicates with geo2ele=" << geo2ele;
    }

    
    return idmap;
  }
  */

  //
  /*
  std::map<uint32_t,uint32_t> HGCalMappingESSourceTester::mapSiPMGeoToElectronics(const hgcal::HGCalMappingModuleParamDeviceCollection &modules,
                                                                                  const hgcal::HGCalMappingCellParamDeviceCollection &cells,
                                                                                  bool geo2ele)
  {
    //loop over SiPM tileboards
    std::map<uint32_t,uint32_t> idmap;
    uint32_t ndups(0);
    for(int i=0; i<modules.view().metadata().size(); i++) {
      
      if(!modules.view()[i].isSiPM()) continue;

      //loop over tiles in this tileboard
      for(int j=0; j<cells.view().metadata().size(); j++) {
        
        // make sure the tile is part of the tileboard
        if(cells.view()[j].t()==-1) continue;
        if(cells.view()[j].type()!=modules.view()[i].type()) continue;

        uint32_t elecid = hgcal::mappingtools::getElectronicsId(modules.view()[i].zside(),
                                                                modules.view()[i].fedid(),
                                                                modules.view()[i].captureblockidx(),
                                                                modules.view()[i].econdidx(),
                                                                cells.view()[j].chip(),
                                                                cells.view()[j].half(),
                                                                cells.view()[j].seq());
        
        uint32_t geoid = hgcal::mappingtools::getSiPMDetId(modules.view()[i].zside(), 
                                                           modules.view()[i].plane(), 
                                                           modules.view()[i].v(), 
                                                           cells.view()[j].iu(), 
                                                           cells.view()[j].iv());
        
        if(geo2ele){
          auto it = idmap.find(geoid);
          ndups += (it != idmap.end());
        }
        if(!geo2ele){
          auto it = idmap.find(elecid);
          ndups += (it != idmap.end());
        }
        
        //map
        idmap[geo2ele ? geoid : elecid] = geo2ele ? elecid : geoid;
      }
    }

    if(ndups>0) {
      edm::LogInfo("HGCalMappingIndexESSourceTester") << "mapSiPMGeoToElectronics found " << ndups << " duplicates with geo2ele=" << geo2ele;
    }
    
    return idmap;
  }
  */

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

// define this as a plug-in
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalMappingESSourceTester);
