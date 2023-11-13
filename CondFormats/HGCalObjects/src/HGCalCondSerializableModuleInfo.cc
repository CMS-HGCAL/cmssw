#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include <algorithm>

//
HGCalModuleInfo HGCalCondSerializableModuleInfo::getModule(int econdidx, int captureblock, int slink, bool zside) const {
  auto _electronicsMatch = [econdidx, captureblock, slink, zside](HGCalModuleInfo m){
    return m.econdidx == econdidx && m.captureblock == captureblock && m.slink == slink && m.zside==zside;
  };
    
  auto it = std::find_if(begin(params_), end(params_), _electronicsMatch);
  if(it==params_.end()) {
    std::cout << zside << " " << econdidx << " " << captureblock << " " << slink << " " << zside << std::endl;
    for(auto mi : params_) {
      std::cout << "\t" << mi.zside << " " << mi.econdidx << " " << mi.captureblock << " " << mi.slink << " " << mi.zside << std::endl;
    }
  }


  return *it;
}

//
HGCalModuleInfo HGCalCondSerializableModuleInfo::getModule(HGCalElectronicsId& id) const {
  return getModule((int)id.econdIdx(), (int)id.captureBlock(), (int)id.fedId(), (bool) id.zSide() );
}

//
HGCalModuleInfo HGCalCondSerializableModuleInfo::getModuleFromGeometry(int plane, int u, int v, bool isSiPM,bool zside) const {
  auto _geometryMatch = [plane, u, v, isSiPM, zside](HGCalModuleInfo m){
    return m.plane == plane && m.u == u && m.v == v && m.isSiPM == isSiPM && m.zside == zside;
  };
  auto it = std::find_if(begin(params_), end(params_), _geometryMatch);
  return *it;
}
  
//
std::tuple<int,int,int,bool> HGCalCondSerializableModuleInfo::getModuleLocation(int econdidx, int captureblock, int slink,bool zside) const {
  auto it = getModule(econdidx,captureblock,slink,zside);
  return std::make_tuple(it.plane, it.u, it.v, it.isSiPM);
}
  
//
std::tuple<int,int,int,bool> HGCalCondSerializableModuleInfo::getModuleLocation(HGCalElectronicsId& id) const {
  return getModuleLocation((int)id.econdIdx(), (int)id.captureBlock(), (int)id.fedId(),id.zSide());
}

//
std::tuple<int,int,int> HGCalCondSerializableModuleInfo::getModuleElectronicsIdentifiers(int plane, int u, int v, bool isSiPM, bool zside) const {
  auto it = getModuleFromGeometry(plane,u,v,isSiPM,zside);
  return std::make_tuple(it.slink, it.captureblock, it.econdidx);
}

//
HGCalElectronicsId HGCalCondSerializableModuleInfo::getModuleElectronicsId(int plane, int u, int v, bool isSiPM, bool zside, uint8_t econderx, uint8_t halfrocch) const {
  auto it = getModuleFromGeometry(plane,u,v,isSiPM,zside);
  return HGCalElectronicsId(it.zside, it.slink, (uint8_t)it.captureblock, (uint8_t)it.econdidx, econderx, halfrocch);
}

//
std::map<HGCalCondSerializableModuleInfo::ModuleInfoKey_t,HGCalCondSerializableModuleInfo::ModuleInfoKey_t> HGCalCondSerializableModuleInfo::getAsSimplifiedModuleLocatorMap(bool elecidAsKey) const {

  std::map<ModuleInfoKey_t,ModuleInfoKey_t> module_keys;
  for(auto m : params_) {
    ModuleInfoKey_t logiKey(m.zside,m.slink,m.captureblock,m.econdidx);
    ModuleInfoKey_t geomKey(m.zside,m.plane,m.u,m.v);
    module_keys[elecidAsKey ? logiKey : geomKey] = elecidAsKey ? geomKey : logiKey;
  }
  
  return module_keys;
}

//
std::tuple<uint16_t,uint16_t,uint16_t,uint16_t> HGCalCondSerializableModuleInfo::getMaxValuesForDenseIndex() const {

  uint16_t maxslink(0),maxcaptureblock(0),maxecondidx(0),maxerx(0);
  for(auto m : params_) {
    maxslink=std::max(m.slink,maxslink);
    maxcaptureblock=std::max(m.captureblock,maxcaptureblock);
    maxecondidx=std::max(m.econdidx,maxecondidx);
    uint16_t nerx=6*(1+m.isHD);
    maxerx=std::max(nerx,maxerx);
  }

  return std::tuple<uint16_t,uint16_t,uint16_t,uint16_t>(maxslink+1,maxcaptureblock+1,maxecondidx+1,maxerx);
}

//
HGCalCondSerializableModuleInfo::ERxBitPatternMap HGCalCondSerializableModuleInfo::getERxBitPattern() const {

  std::tuple<uint16_t,uint16_t,uint16_t,uint16_t> maxValsForDenseIdx=getMaxValuesForDenseIndex();
  uint16_t maxCB=std::get<1>(maxValsForDenseIdx);
  uint16_t maxEcon=std::get<2>(maxValsForDenseIdx);
  
  HGCalCondSerializableModuleInfo::ERxBitPatternMap erxbit;
  for(auto m : params_) {
    uint32_t rtn = erxBitPatternMapDenseIndex(m.slink,m.captureblock,m.econdidx,maxCB,maxEcon);
    uint8_t nerx=6*(1+m.isHD);
    if(!m.isHD and (m.wafType==3 || m.wafType==4)) nerx=3;
    erxbit[rtn]=(1<<nerx)-1;
  }
  return erxbit;
}
