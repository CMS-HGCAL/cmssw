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
  uint32_t halfROCMax{2};             ///< maximum number of half ROC channels
  uint32_t channelSeqMax{36};         ///< maximum number sequence numbers
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

  uint32_t getSize() const{
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

  uint16_t convertSiPMTypecode(std::string typeString) {

    const std::map<std::string, uint16_t> typeMap_ = {
      {"TM-A5A6", 1},
      {"TM-A5", 1},
      {"TM-A6", 1},
      {"TM-B11B12", 2},
      {"TM-B11", 2},
      {"TM-B12", 2},
      {"TM-C5", 3},
      {"TM-D8", 4},
      {"TM-E8", 5},
      {"TM-G3", 6},
      {"TM-G4", 7},
      {"TM-G5", 8},
      {"TM-G6", 9},
      {"TM-G7", 10},
      {"TM-G8", 11},
      {"TM-J8", 12},
      {"TM-J12", 13},
      {"TM-K4", 14},
      {"TM-K5", 15},
      {"TM-K6", 16},
      {"TM-K7", 17},
      {"TM-K8", 18},
      {"TM-K10", 19},
      {"TM-K12", 20}
    };

    auto it = typeMap_.find(typeString);
    if (it != typeMap_.end()) {
      return it->second;
    }
    return typeMap_.size();
  }

  HGCalMappingCellIndexParameters idxParams_;
  
  COND_SERIALIZABLE;
};

#endif
