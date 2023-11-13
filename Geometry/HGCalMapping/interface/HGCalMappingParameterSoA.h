#ifndef Geometry_HGCalMapping_interface_HGCalMappingParameterSoA_h
#define Geometry_HGCalMapping_interface_HGCalMappingParameterSoA_h

#include <Eigen/Core>
#include <Eigen/Dense>
#include <string>

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"
#include "DataFormats/SoATemplate/interface/SoAView.h"

#include "Geometry/HGCalMapping/interface/HGCalMappingParameterIndex.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingCellParameterIndex.h"

namespace hgcal {

  // Generate structure of channel-level arrays (SoA) layout with module mapping information
  GENERATE_SOA_LAYOUT(HGCalMappingModuleParamSoALayout,
                      SOA_SCALAR(HGCalMappingParameterIndex, config),
                      SOA_COLUMN(bool, zside),
                      SOA_COLUMN(bool, isSiPM),
                      SOA_COLUMN(bool, isHD),
                      SOA_COLUMN(int, plane),
                      SOA_COLUMN(int, u),
                      SOA_COLUMN(int, v),
                      SOA_COLUMN(uint16_t, fedid),
                      SOA_COLUMN(uint16_t, localfedid),
                      SOA_COLUMN(uint16_t, wafType),
                      SOA_COLUMN(uint16_t, captureblock),
                      SOA_COLUMN(uint16_t, econdidx),
                      SOA_COLUMN(uint16_t, captureblockidx)
  )
  using HGCalMappingModuleParamSoA = HGCalMappingModuleParamSoALayout<>;

  // Generate structure of channel-level arrays (SoA) layout with silicon cell mapping information
  // GENERATE_SOA_LAYOUT(HGCalMappingSiCellParamSoALayout,
  //                     SOA_SCALAR(HGCalMappingCellParameterIndex, config),
  //                     SOA_COLUMN(bool, isHD),
  //                     SOA_COLUMN(bool, iscalib),
  //                     SOA_COLUMN(uint16_t, type),
  //                     SOA_COLUMN(uint16_t, chip),
  //                     SOA_COLUMN(uint16_t, half),
  //                     SOA_COLUMN(uint16_t, seq),
  //                     SOA_COLUMN(uint16_t, rocpin),
  //                     SOA_COLUMN(int, sicell),
  //                     SOA_COLUMN(int, triglink),
  //                     SOA_COLUMN(int, trigcell),
  //                     SOA_COLUMN(int, iu),
  //                     SOA_COLUMN(int, iv),
  //                     SOA_COLUMN(int, t),
  //                     SOA_COLUMN(float, trace)
  // )
  // using HGCalMappingSiCellParamSoA = HGCalMappingSiCellParamSoALayout<>;

  // // Generate structure of channel-level arrays (SoA) layout with SiPM cell mapping information
  // GENERATE_SOA_LAYOUT(HGCalMappingSiPMCellParamSoALayout,
  //                     SOA_SCALAR(HGCalMappingCellParameterIndex, config),
  //                     SOA_COLUMN(uint16_t, type),
  //                     SOA_COLUMN(uint16_t, seq),  // sequence (sipmcell)
  //                     SOA_COLUMN(int, trigcell), // trigger channel
  //                     SOA_COLUMN(int, triglink), // trigger sum
  //                     SOA_COLUMN(int, iu), // ring
  //                     SOA_COLUMN(int, iv), // phi
  //                     SOA_COLUMN(int, modiu), // module ring
  //                     SOA_COLUMN(int, t)
  // )
  // using HGCalMappingSiPMCellParamSoA = HGCalMappingSiPMCellParamSoALayout<>;

  // Generate structure of channel-level arrays (SoA) layout with cell mapping information for both silicon and SiPM
  GENERATE_SOA_LAYOUT(HGCalMappingCellParamSoALayout,
                      SOA_SCALAR(HGCalMappingCellParameterIndex, config),
                      SOA_COLUMN(bool, isHD),
                      SOA_COLUMN(bool, iscalib),
                      SOA_COLUMN(uint16_t, type),
                      SOA_COLUMN(uint16_t, chip),
                      SOA_COLUMN(uint16_t, half),
                      SOA_COLUMN(uint16_t, seq),
                      SOA_COLUMN(uint16_t, rocpin),
                      SOA_COLUMN(int, sicell),
                      SOA_COLUMN(int, triglink),
                      SOA_COLUMN(int, trigcell),
                      SOA_COLUMN(int, iu),
                      SOA_COLUMN(int, iv),
                      SOA_COLUMN(int, t),
                      SOA_COLUMN(float, trace),
                      SOA_COLUMN(int, modiu) // SiPM module ring
  )
  using HGCalMappingCellParamSoA = HGCalMappingCellParamSoALayout<>;

}  // namespace hgcal

#endif  // Geometry_HGCalMapping_interface_HGCalMappingParameterSoA_h
