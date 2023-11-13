#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include <algorithm>
#include <iostream>

//
std::vector<HGCalSiCellChannelInfo> HGCalCondSerializableSiCellChannelInfo::getAllCellsInModule(bool isHD, uint16_t wafType) const {

  std::vector<HGCalSiCellChannelInfo> wafers;
  std::copy_if(params_.begin(), params_.end(), std::back_inserter(wafers), [&](HGCalSiCellChannelInfo v) {
     return (v.isHD == isHD) && (v.wafType == wafType);
  });
  
  return wafers;
}

//
HGCalSiCellChannelInfo HGCalCondSerializableSiCellChannelInfo::getCellInfo(bool isHD, uint16_t wafType,
                                                                           uint16_t chip, uint16_t half,
                                                                           uint16_t seq) const {

  auto _waferMatch = [isHD, wafType, chip, half, seq](HGCalSiCellChannelInfo m){
     return m.isHD==isHD && m.wafType==wafType && m.chip==chip && m.half==half && m.seq==seq;
  };
    
  auto it = std::find_if(begin(params_), end(params_), _waferMatch);

  return *it;
}

