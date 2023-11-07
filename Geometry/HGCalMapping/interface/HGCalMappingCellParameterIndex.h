#ifndef Geometry_HGCalMapping_interface_HGCalMappingCellParameterIndex_h
#define Geometry_HGCalMapping_interface_HGCalMappingCellParameterIndex_h

#include <cstdint>
#include <vector>
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"

struct HGCalMappingCellParameterIndex {
  // uint32_t maxFEDsPerEndcap{1};     ///< maximum number of FEDs on one side
  // uint32_t sLinkCaptureBlockMax{1};  ///< maximum number of capture blocks in one S-Link
  // uint32_t captureBlockECONDMax{1};  ///< maximum number of ECON-Ds in one capture block
  // uint32_t econdERXMax{12};           ///< maximum number of eRxs in one ECON-D
  // uint32_t erxChannelMax{37};         ///< maximum number of channels in one eRx
  uint32_t moduleTypeMax{1};          ///< maximum number of module types
  uint32_t cellChipMax{6};                ///< maximum number of channel chips
  uint32_t halfROCMax{6};             ///< maximum number of half ROC channels
  uint32_t channelSeqMax{6};                 ///< maximum number sequence numbers


  uint32_t denseIndex(uint32_t type, uint32_t chip, uint32_t half, uint32_t seq) {
    uint32_t rtn = type;
    rtn = rtn * moduleTypeMax + chip;
    rtn = rtn * cellChipMax + half;
    rtn = rtn * halfROCMax + seq;
    return rtn;
  }

  uint32_t getSize(bool roclevel=false) const{
    uint32_t size = moduleTypeMax*cellChipMax*halfROCMax*channelSeqMax;
    return size;
  }

  void setMaxValues(uint32_t typeMax, uint32_t chipMax, uint32_t halfMax, uint32_t seqMax){
    moduleTypeMax = typeMax;
    cellChipMax = chipMax;
    halfROCMax = halfMax;
    channelSeqMax = seqMax;
  }

  uint16_t convertType(std::string typeString) {
    int letterAscii = (int)typeString[0];
    int type;
    if(typeString == "B11B12") {
      type = std::stoi(std::to_string(letterAscii) + "11");
    }
    else if(typeString == "A5A6") {
      type = std::stoi(std::to_string(letterAscii) + "5");
    }
    else {
      type = std::stoi(std::to_string(letterAscii) + typeString.substr(1, typeString.size()-1));
    }
    return (uint16_t)type;
  }

};

#endif