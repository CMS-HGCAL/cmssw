#ifndef CondFormats_HGCalObjects_HGCalCondSerializableSiCellChannelInfo_h
#define CondFormats_HGCalObjects_HGCalCondSerializableSiCellChannelInfo_h

#include <vector>
#include <cstdint>

#include "CondFormats/Serialization/interface/Serializable.h"
/**
   @short representation of a si-cell channel information read from a txt file or db
*/
struct HGCalSiCellChannelInfo {
  bool isHD,iscalib;
  uint16_t wafType, chip, half;
  uint16_t seq,rocpin;
  int sicell,triglink,trigcell,iu,iv,t;
  float trace;
  COND_SERIALIZABLE;
};

/**
   @holder for the si cell channel mappeing
 */
class HGCalCondSerializableSiCellChannelInfo {
  
public:
  
  HGCalCondSerializableSiCellChannelInfo() {}
  virtual ~HGCalCondSerializableSiCellChannelInfo() {}
  inline HGCalCondSerializableSiCellChannelInfo& addParameter(HGCalSiCellChannelInfo &i) {
    params_.push_back(i);
    return *this;
  }
  std::vector<HGCalSiCellChannelInfo> getAllCellsInModule(bool isHD,uint16_t wafType) const;

  HGCalSiCellChannelInfo getCellInfo(bool isHD, uint16_t wafType, uint16_t chip, uint16_t half, uint16_t seq) const;

  std::vector<HGCalSiCellChannelInfo> params_;
  
  COND_SERIALIZABLE;
};

#endif
