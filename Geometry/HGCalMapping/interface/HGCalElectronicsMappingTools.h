#ifndef Geometry_HGCalMapping_interface_HGCalElectronicsMappingTools
#define Geometry_HGCalMapping_interface_HGCalElectronicsMappingTools

#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "DataFormats/ForwardDetId/interface/HGCSiliconDetId.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiPMTileInfo.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"

namespace hgcal {

  /**
     @short method returns a DetId to ElectronicsId map or vice-versa based on the info available for Si cells
   */
  std::map<uint32_t,uint32_t> mapSiGeoToElectronics(const HGCalCondSerializableModuleInfo &, const HGCalCondSerializableSiCellChannelInfo &,bool geo2ele);

  /**
     @short method returns a DetId to ElectronicsId map or vice-versa based on the info available for SiPM tiles
   */
  std::map<uint32_t,uint32_t> mapSiPMGeoToElectronics(const HGCalCondSerializableModuleInfo &, const HGCalCondSerializableSiPMTileInfo &,bool geo2ele);

  /**
     @short returns a map of <ElectronicsId, SiCellChannelInfo index in the module info class>
   */
  std::map<uint32_t,uint32_t> mapSiElectronicsToChannelInfoIdx(const HGCalCondSerializableModuleInfo &moduleInfo,
                                                               const HGCalCondSerializableSiCellChannelInfo &siCellInfo);
  
  /**
     @short formula to get ECOND e-Rx for a given ROC chip/half
   */
  uint16_t getEcondErxFor(uint16_t chip,uint16_t half) { return chip*2+half; }

  /**
     returns a map of electronics id to si channel info
  */
  std::map<uint32_t,HGCalSiCellChannelInfo> mapEleIdToSiInfo(const HGCalCondSerializableModuleInfo &moduleInfo,
                                                             const HGCalCondSerializableSiCellChannelInfo &siCellInfo);
  
};

#endif
