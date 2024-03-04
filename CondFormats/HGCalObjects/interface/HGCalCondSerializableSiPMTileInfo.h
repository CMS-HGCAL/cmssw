#ifndef CondFormats_HGCalObjects_HGCalCondSerializableSiPMTileInfo_h
#define CondFormats_HGCalObjects_HGCalCondSerializableSiPMTileInfo_h

#include <vector>
#include <map>
#include <string>

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/Serialization/interface/Serializable.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"

struct HGCalSiPMTileInfo {
  int sipmcell,plane,iring,iphi,trigch,trigsum,modiring,t;
  std::string type;
  COND_SERIALIZABLE;
};

/**
   @short cond. serializable class holding information on a sipm-on-tile cell
*/
class HGCalCondSerializableSiPMTileInfo {
  
 public:
    
  HGCalCondSerializableSiPMTileInfo() {}
  virtual ~HGCalCondSerializableSiPMTileInfo() {}
  inline HGCalCondSerializableSiPMTileInfo& addParameter(HGCalSiPMTileInfo &i) {
    params_.push_back(i);
    return *this;
  }
  std::vector<HGCalSiPMTileInfo> getAllCellsInModule(int layer, int modiring) const;

  /**
     @short retrieve cell location (plane,ring,iphi) from ROC fields and module location
  */
  std::tuple<int,int,int> getCellLocation(int econderx, int halfrocch, int layer, int modiring);

  /**
     @short retrieve cell location (plane,ring,iphi) from HGCalElectronicsId and module location
  */
  std::tuple<int,int,int> getCellLocation(HGCalElectronicsId& id, int layer, int modiring);

  /**
     @short retrieve DetId from ElectronicsId and Module location, including z-side
  */
  DetId getDetId(HGCalElectronicsId& id, int z, int layer, int modiring, int modiphi);

  /**
     @short retrieve module location (layer,ring,iphi,zside) from DetId
  */
  std::tuple<int,int,int,bool> getModuleLocation(DetId& id);

  /**
     @short retrieve SiPM channel number from ROC fields
  */
  int getSiPMchannel(uint8_t econderx, uint8_t halfrocch);

  /**
     @short retrieve ROC fields (ECON-D eRx, 1/2 ROC channel) from DetId
  */
  std::tuple<uint8_t,uint8_t> getROCFields(DetId& id);

  //parameters to serialize
  std::vector<HGCalSiPMTileInfo> params_;

  COND_SERIALIZABLE;
};

#endif
