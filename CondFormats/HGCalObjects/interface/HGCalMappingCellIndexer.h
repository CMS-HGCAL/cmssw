#ifndef CondFormats_HGCalObjects_interface_HGCalMappingCellParameterIndex_h
#define CondFormats_HGCalObjects_interface_HGCalMappingCellParameterIndex_h

#include <cstdint>
#include <vector>
#include <numeric>
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/Serialization/interface/Serializable.h"
#include "CondFormats/HGCalObjects/interface/HGCalDenseIndexerBase.h"

/**
   @short utility class to assign dense readout cell indexing
 */
class HGCalMappingCellIndexer {

 public:

  typedef HGCalDenseIndexerBase<2> WaferCellDenseIndexer;
  
  HGCalMappingCellIndexer() {}

  /**
     adds to map of type codes (= module types) to handle and updatest the max. number of eRx 
   */
  void processNewCell(std::string typecode, uint16_t chip, uint16_t half) {

    //assign index to this typecode   
    if(typeCodeIndexer_.count(typecode)==0) {
      typeCodeIndexer_[typecode] = typeCodeIndexer_.size(); 
    }

    size_t idx=typeCodeIndexer_[typecode];
    maxErx_[idx]=max(maxErx_[idx],chip*2+half);    
  }

  /**
     @short process the current list of type codes handled and updates the dense indexers
  */
  void update() {

    uint32_t n = typeCodeIndexer_.size();
    offsets_ = std::vector<uint32_t>(n,0);
    di_.resize(n);
    
    for(auto it : typeCodeIndexer_) {
      size_t idx = it->second;
      uint16_t nerx = maxErx_[idx];
      di_[idx].updateRanges( {nerx,37} );
      offsets_[idx]=di_[idx].getMaxIndex();
    }

    offsets_ = std::partial_sum(offsets_.begin(), offsets_.end(), offsets_.begin());
  }

  /**
     @short gets index given typecode string
   */
  size_t getEnumFromTypecode(std::string typecode) {
    auto it = typeCodeIndexer_.find(typecode);
    if( it==typeCodeIndexer_.end())
      throw cms::Exception("ValueError") << " unable to find typecode=" << typecode << " in cell indexer";
    return it->second;
  }

  /**
     @short checks if there is a typecode corresponding to an index
   */
  std::string typecode getTypecodeFromEnum(size_t idx) {
    for(auto it : typeCodeIndexer_)
      if(it.second == idx) return it.first;
    throw cms::Exception("ValueError") << " unable to find typecode corresponding to idx=" << idx;
  }

  /**
     @short returns the dense indexer for a typecode
   */
  WaferCellDenseIndexer getDenseIndexFor(std::string typecode) {
    return getDenseIndexerFor( getEnumFromTypecode(typecode) );
  }

  /**
     @short returns the dense indexer for a given internal index
  */
  WaferCellDenseIndexer getDenseIndexFor(size_t idx) {
    if( idx >= di_.size() )
      throw cms::Exception("ValueError") << " index requested for cell dense indexer (i=" << idx << ") is larger than allocated";
    return di_[idx];
  }

  /**
     @short builders for the dense index
   */
  uint32_t denseIndex(std::string typecode, uint32_t chip, uint32_t half, uint32_t seq) {
    return denseIndex(getEnumFromTypecode(typecode),chip,half,erx,seq);
  }  
  uint32_t denseIndex(std::string typecode, uint32_t erx, uint32_t seq) {
    return denseIndex(getEnumFromTypecode(typecode),erx,seq);
  }
  uint32_t denseIndex(size_t idx, uint32_t chip, uint32_t half, uint32_t seq) {
    uint16_t erx=chip*2+half;
    return denseIndex(idx,erx,seq);
  }
  uint32_t denseIndex(size_t idx, uint32_t erx, uint32_t seq) {
    return di_.denseIndex({erx,seq}) + offsets_[idx];    
  }

  /**
     @short decodes the dense index code
   */
  HGCalElectronicsId elecIdFromIndex(uint32_t rtn, std::string typecode) {
    return elecIdFromIndex(rtn, getEnumFromTypecode(typecode));
  }
  HGCalElectronicsId elecIdFromIndex(uint32_t rtn, size_t idx) {
    if( idx >= di_.size() )
      throw cms::Exception("ValueError") << " index requested for cell dense indexer (i=" << idx << ") is larger than allocated";
    rtn -= offsets_[idx];
    auto rtn_codes = di_[idx].unpackDenseIndex(rtn);
    return HGCalElectronicsId(0, 0, 0, 0, rn_codes[0], rtn_codes[1]);
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

  std::map<std::string,size_t> typeCodeIndexer_;
  std::vector<uint16_t> maxErx_;
  std::vector<uint32_t> offsets_;
  std::vector<WaferCellDenseIndexer> di_;
  
  virtual ~HGCalMappingCellIndexer() {}
  
  COND_SERIALIZABLE;
};

#endif
