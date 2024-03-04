/****************************************************************************
 *
 * This is a part of HGCAL offline software.
 * Authors:
 *   Yulun Miao, Northwestern University
 *
 ****************************************************************************/

#ifndef Geometry_HGCalMapping_HGCalDenseMappingTool
#define Geometry_HGCalMapping_HGCalDenseMappingTool
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"

struct HGCalDenseMappingToolConfig {
  uint32_t maxFEDsPerEndcap{512};     ///< maximum number of FEDs on one side
  uint32_t sLinkCaptureBlockMax{10};  ///< maximum number of capture blocks in one S-Link
  uint32_t captureBlockECONDMax{12};  ///< maximum number of ECON-Ds in one capture block
  uint32_t econdERXMax{12};           ///< maximum number of eRxs in one ECON-D
  uint32_t erxChannelMax{37};         ///< maximum number of channels in one eRx
};

class HGCalDenseMappingTool {
public:
  HGCalDenseMappingTool(HGCalDenseMappingToolConfig);
  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock);
  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND);
  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx);
  uint32_t denseIndex(uint32_t sLink, uint32_t captureBlock, uint32_t eCOND, uint32_t eRx, uint32_t channel);
  uint32_t denseIndex(HGCalElectronicsId elecID);
  HGCalElectronicsId inverseDenseIndex(uint32_t denseIdx);

private:
  HGCalDenseMappingToolConfig config_;
};

#endif