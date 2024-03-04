#include "Geometry/HGCalMapping/interface/HGCalDenseMappingTool.h"

HGCalDenseMappingTool::HGCalDenseMappingTool(HGCalDenseMappingToolConfig config) : config_(config) {}

uint32_t HGCalDenseMappingTool::denseIndex(uint32_t sLink, uint32_t captureBlock) {
  uint32_t rtn = sLink;
  rtn = rtn * config_.sLinkCaptureBlockMax + captureBlock;
  return rtn;
}

uint32_t HGCalDenseMappingTool::denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND) {
  uint32_t rtn = sLink;
  rtn = rtn * config_.sLinkCaptureBlockMax + captureBlock;
  rtn = rtn * config_.captureBlockECONDMax + eCOND;
  return rtn;
}

uint32_t HGCalDenseMappingTool::denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx) {
  uint32_t rtn = sLink;
  rtn = rtn * config_.sLinkCaptureBlockMax + captureBlock;
  rtn = rtn * config_.captureBlockECONDMax + eCOND;
  rtn = rtn * config_.econdERXMax + eRx;
  return rtn;
}

uint32_t HGCalDenseMappingTool::denseIndex(
    uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx, uint32_t channel) {
  uint32_t rtn = sLink;
  rtn = rtn * config_.sLinkCaptureBlockMax + captureBlock;
  rtn = rtn * config_.captureBlockECONDMax + eCOND;
  rtn = rtn * config_.econdERXMax + eRx;
  rtn = rtn * config_.erxChannelMax + channel;
  return rtn;
}

uint32_t HGCalDenseMappingTool::denseIndex(HGCalElectronicsId elecID) {
  return denseIndex(
      elecID.fedId(), elecID.captureBlock(), elecID.econdIdx(), elecID.econdeRx(), elecID.halfrocChannel());
}

HGCalElectronicsId HGCalDenseMappingTool::inverseDenseIndex(uint32_t denseIdx) {
  uint8_t halfrocch = denseIdx % config_.erxChannelMax;
  denseIdx = denseIdx / config_.erxChannelMax;
  uint8_t econderx = denseIdx % config_.econdERXMax;
  denseIdx = denseIdx / config_.econdERXMax;
  uint8_t econdidx = denseIdx % config_.captureBlockECONDMax;
  denseIdx = denseIdx / config_.captureBlockECONDMax;
  uint8_t captureblock = denseIdx % config_.sLinkCaptureBlockMax;
  uint16_t sLink = denseIdx / config_.sLinkCaptureBlockMax;
  bool zside = sLink > config_.maxFEDsPerEndcap;
  return HGCalElectronicsId(zside, sLink, captureblock, econdidx, econderx, halfrocch);
}
