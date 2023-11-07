#ifndef CondFormats_HGCalObjects_HGCalCondSerializableModuleInfo_h
#define CondFormats_HGCalObjects_HGCalCondSerializableModuleInfo_h

#include <vector>
#include <cstdint>
#include <tuple>

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/Serialization/interface/Serializable.h"

/**
   @short representation of a si-cell channel information read from a txt file or db
*/
struct HGCalModuleInfo {
  bool zside,isSiPM,isHD;
  int plane, u, v;
  uint16_t fedid,slink,wafType,captureblock,econdidx,captureblockidx;
  COND_SERIALIZABLE;
};

/**
   @holder for the si cell channel mapping
 */
class HGCalCondSerializableModuleInfo {
  
public:

  typedef std::tuple<bool,int,int,int> ModuleInfoKey_t;
  typedef std::map<uint32_t, uint16_t> ERxBitPatternMap;
  
  HGCalCondSerializableModuleInfo() {}
  virtual ~HGCalCondSerializableModuleInfo() {}

  /**
     @short add module to collection
   */
  inline HGCalCondSerializableModuleInfo& addParameter(HGCalModuleInfo &i) {
    params_.push_back(i);
    return *this;
  }

  /**
     @short retrieve module info from electronics id information (ECON-D idx, Capture Block idx, FED ID, z-side)
  */
  HGCalModuleInfo getModule(int ,int ,int,bool) const;

  /**
     @short retrieve module info from electronics id
  */
  HGCalModuleInfo getModule(HGCalElectronicsId& ) const;

  /**
     @short retrieve module info from geometry
  */
  HGCalModuleInfo getModuleFromGeometry(int ,int ,int ,bool ,bool ) const;
  
  /**
     @short Module location (plane,u,v,isSiPM) from electronics id information (ECON-D idx, Capture Block idx, FED ID,z-side)
  */
  std::tuple<int,int,int,bool> getModuleLocation(int ,int ,int, bool) const;
  
  /**
     @short Module location (plane,u,v,isSiPM) from ElectronicsId 
  */
  std::tuple<int,int,int,bool> getModuleLocation(HGCalElectronicsId& ) const;

  /*
    @short module electronics identifier (ECON-D idx, Capture Block idx, FED ID,z-side)
  */
  std::tuple<int,int,int> getModuleElectronicsIdentifiers(int ,int ,int ,bool , bool ) const;

   /*
    @short retireve HGCalElectronicsId from geometry and channel ROC fields
  */
  HGCalElectronicsId getModuleElectronicsId(int ,int ,int ,bool , bool ,uint8_t ,uint8_t ) const;

  /**
     @short retrieves a map elecId <-> geomId
     (zside,slink,captureblock,econd) <-> (zside, plane, u, v)
     elecidAsKey - if false the geomId is used as the key
   */
  std::map<ModuleInfoKey_t,ModuleInfoKey_t> getAsSimplifiedModuleLocatorMap(bool elecAsKey=true) const;

  /**
     @short returns <max s-link, max capture block, max econ-d idx, max eRx> to build a dense index
   */
  std::tuple<uint16_t,uint16_t,uint16_t,uint16_t> getMaxValuesForDenseIndex() const;

  /**
     @short returns s-link index for a fedid
   */
  std::map<uint16_t,uint16_t> getFedToSlinkMap() {
    std::map<uint16_t,uint16_t> fed2slink;
    for(auto m : params_) fed2slink[m.fedid]=m.slink;
    return fed2slink;
  }
  
  /**
     @short returns eRxBitPattern
   */
  ERxBitPatternMap getERxBitPattern() const;
  
  /**
     @short computes the dense index for the erx bit pattern
   */
  static uint32_t erxBitPatternMapDenseIndex(uint16_t slink, uint16_t captureblock, uint16_t econdidx, uint16_t maxCB,uint16_t maxEcon){
    uint32_t rtn=slink * maxCB + captureblock;
    rtn = rtn * maxEcon + econdidx;
    return rtn;
  }
  
  //parameters to serialize
  std::vector<HGCalModuleInfo> params_;

  COND_SERIALIZABLE;
};

#endif
