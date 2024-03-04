#include "Geometry/HGCalMapping/interface/HGCalElectronicsMappingTools.h"

namespace hgcal {
  
  std::map<uint32_t,uint32_t> mapSiGeoToElectronics(const HGCalCondSerializableModuleInfo &moduleInfo,
                                                    const HGCalCondSerializableSiCellChannelInfo &siCellInfo,
                                                    bool geo2ele)
  {

    //loop over Si modules
    std::map<uint32_t,uint32_t> idmap;

    for(auto m : moduleInfo.params_){
      
      if(m.isSiPM) continue;
      
      auto isHD=m.isHD;
      auto wafType=m.wafType;

      //loop over cells in this module skipping calibration and unconnected channels
      auto cells=siCellInfo.getAllCellsInModule(isHD,wafType);
      for(auto c : cells) {
        
        if(c.t!=1) continue;

        //build the electronics id
        auto econderx = hgcal::getEcondErxFor(c.chip,c.half);
        uint32_t elecid = HGCalElectronicsId(m.zside,m.slink,m.captureblock,m.econdidx,econderx,c.seq).raw(); 

        //build the detector id
        DetId::Detector det = m.plane<=26 ? DetId::Detector::HGCalEE : DetId::Detector::HGCalHSi;
        int zp(m.zside ?  1 : -1);
        int celltype(0); //fix me... this is related to thickness...
        uint32_t geoid = HGCSiliconDetId(det,zp,celltype,m.plane,m.u,m.v,c.iu,c.iv).rawId();

        //map
        idmap[geo2ele ? geoid : elecid] = geo2ele ? elecid : geoid;
      }
    }
    
    return idmap;
  }

  //
  std::map<uint32_t,HGCalSiCellChannelInfo> mapEleIdToSiInfo(const HGCalCondSerializableModuleInfo &moduleInfo,
                                                             const HGCalCondSerializableSiCellChannelInfo &siCellInfo)
  {
    
    //loop over Si modules
    std::map<uint32_t,HGCalSiCellChannelInfo> infomap;

    for(auto m : moduleInfo.params_){
      
      if(m.isSiPM) continue;
      
      auto isHD=m.isHD;
      auto wafType=m.wafType;

      //loop over cells in this module skipping calibration and unconnected channels
      auto cells=siCellInfo.getAllCellsInModule(isHD,wafType);
      for(auto c : cells) {
        
        //build the electronics id
        auto econderx = hgcal::getEcondErxFor(c.chip,c.half);
        uint32_t elecid = HGCalElectronicsId(m.zside,m.slink,m.captureblock,m.econdidx,econderx,c.seq).raw(); 

        infomap[elecid] = c;
      }
    }

    return infomap;
  }

  //
  std::map<uint32_t,uint32_t> mapSiPMGeoToElectronics(const HGCalCondSerializableModuleInfo &moduleInfo,
                                                    const HGCalCondSerializableSiPMTileInfo &sipmCellInfo,
                                                    bool geo2ele)
  {
    //loop over SiPM tileboards
    std::map<uint32_t,uint32_t> idmap;
    for(auto m : moduleInfo.params_){
      
      if(!m.isSiPM) continue;

      //loop over tiles in this tileboard
      auto cells = sipmCellInfo.getAllCellsInModule(m.plane, m.u);
      for(auto c: cells){
        
        if(c.t!=1) continue;

        uint16_t econderx = c.sipmcell/36;
        uint16_t halfrocch = c.sipmcell - 36*econderx;
        //fedid -> slink?
        uint32_t elecid = HGCalElectronicsId(m.zside,m.fedid,m.captureblock,m.econdidx,econderx,halfrocch).raw();

        int layer = m.plane - 25;
        int type = ((layer <= 8) ? 0 : ((layer <= 17) ? 1 : 2));
        int ring = (m.zside ? c.iring : (-1)*c.iring);
        int iphi = m.v*8 + c.iphi + 1;

        uint32_t geoid = HGCScintillatorDetId(type, layer, ring, iphi, false, true).rawId();

        //map
        idmap[geo2ele ? geoid : elecid] = geo2ele ? elecid : geoid;
      }
    }

    return idmap;
  }

  //
  std::map<uint32_t,uint32_t> mapSiElectronicsToChannelInfoIdx(const HGCalCondSerializableModuleInfo &moduleInfo,
                                                               const HGCalCondSerializableSiCellChannelInfo &siCellInfo) {

    //loop over Si modules
    std::map<uint32_t,uint32_t> idmap;
    
    for(auto m : moduleInfo.params_){
      
      if(m.isSiPM) continue;
      
      //loop over cells in this module
      auto cells=siCellInfo.getAllCellsInModule(m.isHD,m.wafType);
      for(uint32_t idx=0; idx<cells.size(); idx++) {
        
        //build the electronics id
        auto c = cells[idx];
        auto econderx = hgcal::getEcondErxFor(c.chip,c.half);
        uint32_t id_key = HGCalElectronicsId(m.zside,m.slink,m.captureblock,m.econdidx,econderx,c.seq).raw(); 

        //map
        idmap[id_key] = idx;
      }
    }
    
    return idmap;
  }

  
}
