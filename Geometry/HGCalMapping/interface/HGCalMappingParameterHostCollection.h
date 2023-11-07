#ifndef Geometry_HGCalMapping_interface_HGCalMappingParameterHostCollection_h
#define Geometry_HGCalMapping_interface_HGCalMappingParameterHostCollection_h

#include "DataFormats/Portable/interface/PortableHostCollection.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingParameterSoA.h"

namespace hgcal {

  // SoA with channel-level module mapping parameters in host memory:
  //   zside, isSiPM, isHD, plane, u, v,
  //   fedid, slink, wafType, captureblock, econdidx, captureblockidx
  using HGCalMappingModuleParamHostCollection = PortableHostCollection<HGCalMappingModuleParamSoA>;

  // SoA with channel-level silicon cell mapping parameters in host memory:
  //   isHD, iscalib, wafType, chip, half, seq, rocpin,
  //   sicell, triglink, trigcell, iu, iv,t, trace
  // using HGCalMappingSiCellParamHostCollection = PortableHostCollection<HGCalMappingSiCellParamSoA>;

  // SoA with channel-level SiPM cell mapping parameters in host memory:
  //   sipmcell, plane, iring, iphi, trigch, trigsum, modiring, t, type
  // using HGCalMappingSiPMCellParamHostCollection = PortableHostCollection<HGCalMappingSiPMCellParamSoA>;

  // SoA with channel-level cell mapping parameters in host memory for both Si and SiPM channels:
  //   sipmcell, plane, iring, iphi, trigch, trigsum, modiring, t, type
  using HGCalMappingCellParamHostCollection = PortableHostCollection<HGCalMappingCellParamSoA>;

}  // namespace hgcal

#endif  // Geometry_HGCalMapping_interface_HGCalMappingParameterHostCollection_h