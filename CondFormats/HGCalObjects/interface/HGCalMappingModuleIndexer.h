#ifndef CondFormats_HGCalObjects_interface_HGCalMappingParameterIndex_h
#define CondFormats_HGCalObjects_interface_HGCalMappingParameterIndex_h

#include <cstdint>
#include <vector>
#include <algorithm>

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/Serialization/interface/Serializable.h"
#include "CondFormats/HGCalObjects/interface/HGCalDenseIndexerBase.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"

struct FEDReadoutSequence_t {
  uint32_t id;
  std::vector<int> readoutTypes_;
  std::vector<uint32_t> modOffsets_, erxOffsets_, chDataOffsets_;
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
  void processNewModule(uint32_t fedid, uint16_t captureblockIdx, uint16_t econdIdx,uint32_t typecodeIdx, uint32_t nerx, uint32_t nwords) {

    //add fed if needed
    if(fedid>=fedReadoutSequences_.size()) {
      fedReadoutSequences_.resize(fedid+1);
    }
    FEDReadoutSequence_t &frs = fedReadoutSequences_[fedid];
    frs.id=fedid;

    //assign position, resize if needed, and fill the type code
    HGCalDenseIndexerBase mi( {maxCBperFED_,maxECONDperCB_} );
    uint32_t idx = mi.denseIndex({{captureblockIdx,econdIdx}});
    if(idx >= frs.readoutTypes_.size()) {
      frs.readoutTypes_.resize(idx+1,-1);
    }
    frs.readoutTypes_[idx] = typecodeIdx;

    //count another typecodein the global list
    if(typecodeIdx>=globalTypesCounter_.size()) {
      globalTypesCounter_.resize(typecodeIdx+1,0);
      globalTypesNErx_.resize(typecodeIdx+1,0);
      globalTypesNWords_.resize(typecodeIdx+1,0);
      dataOffsets_.resize(typecodeIdx+1,0);
    }
    globalTypesCounter_[typecodeIdx]++;
    globalTypesNErx_[typecodeIdx]=nerx;
    globalTypesNWords_[typecodeIdx]=nwords;
  }


  /**
     @short
   */
  void finalize() {

    //max indices at different levels
    nfeds_=fedReadoutSequences_.size();
    maxModulesIdx_ = std::accumulate(globalTypesCounter_.begin(), globalTypesCounter_.end(),0);
    maxErxIdx_ = std::inner_product(globalTypesCounter_.begin(), globalTypesCounter_.end(), globalTypesNErx_.begin(), 0);
    maxDataIdx_ = std::inner_product(globalTypesCounter_.begin(), globalTypesCounter_.end(), globalTypesNWords_.begin(), 0);
      
    //compute the global offset to assign per board type, eRx and channel data
    moduleOffsets_.resize(maxModulesIdx_,0);
    erxOffsets_.resize(maxModulesIdx_,0);
    dataOffsets_.resize(maxModulesIdx_,0);
    for(size_t i=1; i<globalTypesCounter_.size(); i++) {
      moduleOffsets_[i] = globalTypesCounter_[i-1];
      erxOffsets_[i] = globalTypesCounter_[i-1]*globalTypesNErx_[i-1];
      dataOffsets_[i] = globalTypesCounter_[i-1]*globalTypesNWords_[i-1];
    }      
    std::partial_sum(moduleOffsets_.begin(), moduleOffsets_.end(), moduleOffsets_.begin());
    std::partial_sum(erxOffsets_.begin(),    erxOffsets_.end(),    erxOffsets_.begin());
    std::partial_sum(dataOffsets_.begin(),   dataOffsets_.end(),   dataOffsets_.begin());
    
    //now go through the FEDs and ascribe the offsets per module in the readout sequence
    std::vector<uint32_t > typeCounters(globalTypesCounter_.size(),0);
    for(auto &fedit : fedReadoutSequences_) {

      //build the final, compact readout sequence
      std::remove_if ( fedit.readoutTypes_.begin(),
                       fedit.readoutTypes_.end(),
                       [&](int val) -> bool { return val==-1; } );

      //assign the final offsets at the different levels
      size_t nmods=fedit.readoutTypes_.size();
      fedit.modOffsets_.resize(nmods,0);
      fedit.erxOffsets_.resize(nmods,0);
      fedit.chDataOffsets_.resize(nmods,0);

      //an internal counter of the modules types in this fed is needed (this will not be persisted)
      //std::map<int,uint32_t> internal_modCounters;
      //std::transform( fedit.readoutTypes_.begin(),
      //                fedit.readoutTypes_.end(),
      //               std::inserter( internal_modCounters, internal_modCounters.begin() ),
      //               []( int t ){ return std::pair<int, uint32_t>( t, 0 ); } );
      for(size_t i=0; i<nmods; i++) {
        
        uint32_t type_val = fedit.readoutTypes_[i];

        //module offset : global offset for this type + current index for this type
        uint32_t baseMod_offset = moduleOffsets_[type_val]+typeCounters[type_val];
        //uint32_t internalMod_offset = internal_modCounters[type_val];
        fedit.modOffsets_[i] = baseMod_offset;// + internalMod_offset;

        //erx-level offset : global offset of e-Rx of this type + #e-Rrx * current index for this type
        uint32_t baseErx_offset = erxOffsets_[type_val];
        uint32_t internalErx_offset = globalTypesNErx_[type_val]*typeCounters[type_val];
        fedit.erxOffsets_[i] = baseErx_offset + internalErx_offset;

        //channel data offset: global offset for data of this type + #words * current index for this type
        uint32_t baseData_offset = dataOffsets_[type_val];
        uint32_t internalData_offset = globalTypesNWords_[type_val]*typeCounters[type_val];
        fedit.chDataOffsets_[i] = baseData_offset + internalData_offset;

        //internal_modCounters[type_val]++;
        typeCounters[type_val]++;
      }
    }

  }

  /**
     @short returns the index for the n-th module in the readout sequence of a FED
   */
  uint32_t getIndexForModule(uint32_t fedid, uint32_t nmod) {
    if(fedid>nfeds_ || fedReadoutSequences_[fedid].modOffsets_.size()<nmod)
      throw cms::Exception("ValueError") << "FED ID=" << fedid << " or #module requested (=" << nmod << ") is unknown to current mapping";    
    return fedReadoutSequences_[fedid].modOffsets_[nmod];
  };
  uint32_t getIndexForModuleErx(uint32_t fedid, uint32_t nmod, uint32_t erxidx) {
    if(fedid>nfeds_ || fedReadoutSequences_[fedid].erxOffsets_.size()<nmod)
      throw cms::Exception("ValueError") << "FED ID=" << fedid << " or #module requested (=" << nmod << ") is unknown to current mapping";    
    return fedReadoutSequences_[fedid].erxOffsets_[nmod]+erxidx;
  };
  uint32_t getIndexForModuleData(uint32_t fedid, uint32_t nmod,uint32_t erxidx,uint32_t chidx) {
    if(fedid>nfeds_ || fedReadoutSequences_[fedid].chDataOffsets_.size()<nmod)
      throw cms::Exception("ValueError") << "FED ID=" << fedid << " or #module requested (=" << nmod << ") is unknown to current mapping";    
    return fedReadoutSequences_[fedid].chDataOffsets_[nmod]+erxidx*HGCalMappingCellIndexer::maxChPerErx_+chidx;
  };

  std::vector<FEDReadoutSequence_t> fedReadoutSequences_;                        ///< the sequence of FED readout sequence descriptors
  std::vector<uint32_t> globalTypesCounter_,globalTypesNErx_,globalTypesNWords_; ///< global counters for types of modules, number of e-Rx and words
  std::vector<uint32_t> moduleOffsets_,erxOffsets_,dataOffsets_;                 ///< base offsets to apply per module type with different granularity : module, e-Rx, channel data
  uint32_t nfeds_,maxDataIdx_,maxErxIdx_,maxModulesIdx_;                         ///< global counters (sizes of vectors)

  constexpr static uint32_t maxCBperFED_ = 10;    ///< max number of main buffers/capture blocks per FED
  constexpr static uint32_t maxECONDperCB_ = 12;  ///< max number of ECON-Ds processed by a main buffer/capture block

  COND_SERIALIZABLE;
};

#endif
