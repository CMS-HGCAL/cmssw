#ifndef CondFormats_HGCalObjects_interface_HGCalMappingCellParameterIndex_h
#define CondFormats_HGCalObjects_interface_HGCalMappingCellParameterIndex_h

#include <cstdint>
#include <vector>
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/Serialization/interface/Serializable.h"

/**
   @short holds the parameters needed to compute the dense indexing for HGCAL readout cells
 */
struct HGCalMappingCellIndexParameters {
  uint32_t moduleTypeMax{1};          ///< maximum number of module types
  uint32_t cellChipMax{6};            ///< maximum number of channel chips
  uint32_t halfROCMax{6};             ///< maximum number of half ROC channels
  uint32_t channelSeqMax{6};          ///< maximum number sequence numbers
  COND_SERIALIZABLE;
};

/**
   @short utility class to assign dense readout cell indexing
 */
class HGCalMappingCellIndexer {

 public:

  HGCalMappingCellIndexer() {}

  virtual ~HGCalMappingCellIndexer() {}

  uint32_t denseIndex(uint32_t type, uint32_t chip, uint32_t half, uint32_t seq) {
    uint32_t rtn = type;
    rtn = rtn * idxParams_.moduleTypeMax + chip;
    rtn = rtn * idxParams_.cellChipMax + half;
    rtn = rtn * idxParams_.halfROCMax + seq;
    return rtn;
  }

  uint32_t getSize(bool roclevel=false) const{
    uint32_t size = idxParams_.moduleTypeMax*idxParams_.cellChipMax*idxParams_.halfROCMax*idxParams_.channelSeqMax;
    return size;
  }

  inline void update(const HGCalMappingCellIndexParameters &from) { idxParams_ = from; }
  
  void update(uint32_t typeMax, uint32_t chipMax, uint32_t halfMax, uint32_t seqMax){
    idxParams_.moduleTypeMax = typeMax;
    idxParams_.cellChipMax = chipMax;
    idxParams_.halfROCMax = halfMax;
    idxParams_.channelSeqMax = seqMax;
  }

  uint16_t convertType(std::string typeString) {
    if(typeString == "B11B12") {
      return 44;
    }
    if(typeString == "A5A6") {
      return 15;
    }
    //Assuming all other types are stings with two characters, eg. "D8"
    char firstChar = typeString[0];
    char secondChar = typeString[1];
    return (firstChar - 'A' + 3) * (secondChar - '0' + 3);
  }

  HGCalMappingCellIndexParameters idxParams_;
  
  COND_SERIALIZABLE;
};

#endif
