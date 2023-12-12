#ifndef CondFormats_HGCalObjects_interface_HGCalMappingParameterIndex_h
#define CondFormats_HGCalObjects_interface_HGCalMappingParameterIndex_h

#include <cstdint>
#include <vector>

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/Serialization/interface/Serializable.h"

/**
   @short holds the parameters needed to compute the dense indexing for HGCAL modules
*/
struct HGCalMappingModuleIndexParameters {
  uint32_t maxFEDsPerEndcap{512};     ///< maximum number of FEDs on one side
  uint32_t sLinkCaptureBlockMax{10};  ///< maximum number of capture blocks in one S-Link
  uint32_t captureBlockECONDMax{12};  ///< maximum number of ECON-Ds in one capture block
  uint32_t econdERXMax{12};           ///< maximum number of eRxs in one ECON-D
  uint32_t erxChannelMax{37};         ///< maximum number of channels in one eRx

  COND_SERIALIZABLE;
};


/**
   @short utility class to assign dense readout module indexing
 */
class HGCalMappingModuleIndexer {

 public:

  HGCalMappingModuleIndexer() {}

  virtual ~HGCalMappingModuleIndexer() {}

  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock) {
    uint32_t rtn = sLink;
    rtn = rtn * idxParams_.sLinkCaptureBlockMax + captureBlock;
    return rtn;
  }

  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND) {
    uint32_t rtn = sLink;
    rtn = rtn * idxParams_.sLinkCaptureBlockMax + captureBlock;
    rtn = rtn * idxParams_.captureBlockECONDMax + eCOND;
    return rtn;
  }

  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx) {
    uint32_t rtn = sLink;
    rtn = rtn * idxParams_.sLinkCaptureBlockMax + captureBlock;
    rtn = rtn * idxParams_.captureBlockECONDMax + eCOND;
    rtn = rtn * idxParams_.econdERXMax + eRx;
    return rtn;
  }

  uint32_t denseIndex(
    uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx, uint32_t channel) {
    uint32_t rtn = sLink;
    rtn = rtn * idxParams_.sLinkCaptureBlockMax + captureBlock;
    rtn = rtn * idxParams_.captureBlockECONDMax + eCOND;
    rtn = rtn * idxParams_.econdERXMax + eRx;
    rtn = rtn * idxParams_.erxChannelMax + channel;
    return rtn;
  }

  uint32_t denseIndex(HGCalElectronicsId elecID) {
    return denseIndex(
      elecID.localFEDId(), elecID.captureBlock(), elecID.econdIdx(), elecID.econdeRx(), elecID.halfrocChannel());
  }

  HGCalElectronicsId inverseDenseIndex(uint32_t denseIdx) {
    uint8_t halfrocch = denseIdx % idxParams_.erxChannelMax;
    denseIdx = denseIdx / idxParams_.erxChannelMax;
    uint8_t econderx = denseIdx % idxParams_.econdERXMax;
    denseIdx = denseIdx / idxParams_.econdERXMax;
    uint8_t econdidx = denseIdx % idxParams_.captureBlockECONDMax;
    denseIdx = denseIdx / idxParams_.captureBlockECONDMax;
    uint8_t captureblock = denseIdx % idxParams_.sLinkCaptureBlockMax;
    uint16_t sLink = denseIdx / idxParams_.sLinkCaptureBlockMax;
    bool zside = sLink > idxParams_.maxFEDsPerEndcap;
    return HGCalElectronicsId(zside, sLink, captureblock, econdidx, econderx, halfrocch);
  }

  inline void update(const HGCalMappingModuleIndexParameters &from) { idxParams_ = from; }
  
  void update(uint32_t maxslink, uint32_t maxcaptureblock, uint32_t maxecondidx, uint32_t maxerx, const int commonMode=2) {
    idxParams_.maxFEDsPerEndcap = maxslink;   
    idxParams_.sLinkCaptureBlockMax = maxcaptureblock;
    idxParams_.captureBlockECONDMax = maxecondidx;
    idxParams_.econdERXMax = maxerx;         
    idxParams_.erxChannelMax = 37 + commonMode; // +2 for the two common modes
  }

  constexpr uint32_t getSize(bool roclevel=false) const{
    uint32_t size = idxParams_.maxFEDsPerEndcap*idxParams_.sLinkCaptureBlockMax*idxParams_.captureBlockECONDMax*idxParams_.econdERXMax;
    if(!roclevel) // channel-level: include channel size
      size *= idxParams_.erxChannelMax;
    return size;
  }

  uint16_t convertSiTypecode(std::string typeString) {

    const std::map<char, uint16_t> typeMap_ = {
      {'F', 0},
      {'T', 1},
      {'B', 2},
      {'L', 3},
      {'R', 4},
      {'5', 5}
    };

    auto it = typeMap_.find(typeString[3]);
    if (it != typeMap_.end()) {
      return it->second;
    }
    return typeMap_.size();
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

  HGCalMappingModuleIndexParameters idxParams_;
  
  COND_SERIALIZABLE;
};

#endif
