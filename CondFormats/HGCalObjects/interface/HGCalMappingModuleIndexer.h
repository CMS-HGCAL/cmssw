#ifndef CondFormats_HGCalObjects_interface_HGCalMappingParameterIndex_h
#define CondFormats_HGCalObjects_interface_HGCalMappingParameterIndex_h

#include <cstdint>
#include <vector>
#include <algorithm>

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/Serialization/interface/Serializable.h"
#include "CondFormats/HGCalObjects/interface/HGCalDenseIndexerBase.h"

struct FEDReadoutSequence_t {
  std::vector<int> readoutTypes_;
  std::vector<int> readoutOffsets_;
  COND_SERIALIZABLE;
};


/**
   @short utility class to assign dense readout module indexing
   the class holds the information on the expected readout sequence (module types) per FED and their offset in the SoAs of data
 */
class HGCalMappingModuleIndexer {

public:
    
  HGCalMappingModuleIndexer() { }
  
  virtual ~HGCalMappingModuleIndexer() {}
  

  /**
     @short for a new module it adds it's type to the readaout sequence vector
     if the fed id is not yet existing in the mapping it's added
     a dense indexer is used to create the necessary indices for the new module
     unused indices will be set with -1
   */
  void processNewModule(uint32_t fedid, uint16_t captureblockIdx, uint16_t econdIdx,uint32_t typecodeIdx, uint32_t nwords) {

    //add fed if needed
    fedReadoutSequences_.resize(fedid);
    FEDReadoutSequence_t &frs = fedReadoutSequences_[fedid];

    //assign position, resize if needed and fill the type code
    HGCalDenseIndexerBase mi( {maxCBperFED_,maxECONDperCB_} );
    uint32_t idx = mi.denseIndex({{captureblockIdx,econdIdx}});
    frs.readoutTypes_.resize(idx+1,-1);    
    frs.readoutTypes_[idx] = typecodeIdx;

    //increment global counter per type
    globalTypesCounter_.resize(typecodeIdx+1,0);    
    globalTypesCounter_[typecodeIdx]++;
    globalTypesNWords_.resize(typecodeIdx+1,0);
    globalTypesNWords_[typecodeIdx]=nwords;
    globalTypesOffsets_.resize(typecodeIdx+1,0);
  }


  /**
     @short
   */
  void finalize() {

    //compute the global offset to assign per board type
    for(size_t i=1; i<globalTypesCounter_.size(); i++) {
      globalTypesOffsets_[i] = globalTypesCounter_[i-1]*globalTypesNWords_[i-1];
    }
    std::partial_sum(globalTypesOffsets_.begin(), globalTypesOffsets_.end(), globalTypesOffsets_.begin());

    //max index which will be needed to allocate memory for
    maxIdx_ = std::inner_product(globalTypesCounter_.begin(), globalTypesCounter_.end(), globalTypesNWords_.begin(), 0);

    //now go through the FEDs and ascribe the offsets per module in the readout sequence
    std::vector<uint32_t > typeCounters(globalTypesCounter_.size(),0);
    nfeds_=fedReadoutSequences_.size();
    for(auto fedit : fedReadoutSequences_) {

      //build the final, compact readout sequence
      std::remove_if ( fedit.readoutTypes_.begin(),
                       fedit.readoutTypes_.end(),
                       [&](int val) -> bool { return val==-1; } );
      
      //assign offsets
      size_t nmods=fedit.readoutTypes_.size();
      fedit.readoutOffsets_.resize(nmods,0);
      for(size_t i=0; i<nmods; i++) {
        
        uint32_t type_val = fedit.readoutTypes_[i];
        uint32_t base_offset=globalTypesOffsets_[type_val];
        uint32_t intern_offset=globalTypesNWords_[type_val]*typeCounters[type_val];
        fedit.readoutOffsets_[i] = intern_offset+base_offset;
        
        typeCounters[type_val]++;
      }
    }
    
  }

  /**
     @short returns the index for the n-th module in the readout sequence of a FED
   */
  uint32_t getIndexForModule(uint32_t fedid, uint32_t nmod) {
    if(fedid>nfeds_ || fedReadoutSequences_[fedid].readoutOffsets_.size()<nmod)
      throw cms::Exception("ValueError") << "FED ID=" << fedid << " or #module requested (=" << nmod << ") is unknown to current mapping";    
    return fedReadoutSequences_[fedid].readoutOffsets_[nmod];
  };

  std::vector<FEDReadoutSequence_t> fedReadoutSequences_;
  std::vector<uint32_t> globalTypesCounter_,globalTypesNWords_,globalTypesOffsets_;
  uint32_t nfeds_,maxIdx_;

  constexpr static uint32_t maxCBperFED_ = 10;
  constexpr static uint32_t maxECONDperCB_ = 12;

  COND_SERIALIZABLE;
};

#endif
