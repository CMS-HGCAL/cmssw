#ifndef Geometry_HGCalMapping_interface_HGCalMappingParameterIndex_h
#define Geometry_HGCalMapping_interface_HGCalMappingParameterIndex_h

#include <cstdint>
#include <vector>
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"

struct HGCalMappingParameterIndex {
  uint32_t maxFEDsPerEndcap{512};     ///< maximum number of FEDs on one side
  uint32_t sLinkCaptureBlockMax{10};  ///< maximum number of capture blocks in one S-Link
  uint32_t captureBlockECONDMax{12};  ///< maximum number of ECON-Ds in one capture block
  uint32_t econdERXMax{12};           ///< maximum number of eRxs in one ECON-D
  uint32_t erxChannelMax{37};         ///< maximum number of channels in one eRx


  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock) {
    uint32_t rtn = sLink;
    rtn = rtn * sLinkCaptureBlockMax + captureBlock;
    return rtn;
  }

  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND) {
    uint32_t rtn = sLink;
    rtn = rtn * sLinkCaptureBlockMax + captureBlock;
    rtn = rtn * captureBlockECONDMax + eCOND;
    return rtn;
  }

  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx) {
    uint32_t rtn = sLink;
    rtn = rtn * sLinkCaptureBlockMax + captureBlock;
    rtn = rtn * captureBlockECONDMax + eCOND;
    rtn = rtn * econdERXMax + eRx;
    return rtn;
  }

  uint32_t denseIndex(
    uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx, uint32_t channel) {
    uint32_t rtn = sLink;
    rtn = rtn * sLinkCaptureBlockMax + captureBlock;
    rtn = rtn * captureBlockECONDMax + eCOND;
    rtn = rtn * econdERXMax + eRx;
    rtn = rtn * erxChannelMax + channel;
    return rtn;
  }

  uint32_t denseIndex(HGCalElectronicsId elecID) {
    return denseIndex(
      elecID.fedId(), elecID.captureBlock(), elecID.econdIdx(), elecID.econdeRx(), elecID.halfrocChannel());
  }

  HGCalElectronicsId inverseDenseIndex(uint32_t denseIdx) {
    uint8_t halfrocch = denseIdx % erxChannelMax;
    denseIdx = denseIdx / erxChannelMax;
    uint8_t econderx = denseIdx % econdERXMax;
    denseIdx = denseIdx / econdERXMax;
    uint8_t econdidx = denseIdx % captureBlockECONDMax;
    denseIdx = denseIdx / captureBlockECONDMax;
    uint8_t captureblock = denseIdx % sLinkCaptureBlockMax;
    uint16_t sLink = denseIdx / sLinkCaptureBlockMax;
    bool zside = sLink > maxFEDsPerEndcap;
    return HGCalElectronicsId(zside, sLink, captureblock, econdidx, econderx, halfrocch);
  }

  void setMaxValues(uint32_t maxslink, uint32_t maxcaptureblock, uint32_t maxecondidx, uint32_t maxerx, const int commonMode=2) {
    maxFEDsPerEndcap = maxslink;   
    sLinkCaptureBlockMax = maxcaptureblock;
    captureBlockECONDMax = maxecondidx;
    econdERXMax = maxerx;         
    erxChannelMax = 37 + commonMode; // +2 for the two common modes
  }

  constexpr uint32_t getSize(bool roclevel=false) const{
    uint32_t size = maxFEDsPerEndcap*sLinkCaptureBlockMax*captureBlockECONDMax*econdERXMax;
    if(!roclevel) // channel-level: include channel size
      size *= erxChannelMax;
    return size;
  }

};

#endif