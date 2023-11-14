#ifndef CondFormats_HGCalObjects_interface_HGCalMappingParameterSoA_h
#define CondFormats_HGCalObjects_interface_HGCalMappingParameterSoA_h

#include <Eigen/Core>
#include <Eigen/Dense>
#include <string>

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"
#include "DataFormats/SoATemplate/interface/SoAView.h"

#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"

namespace hgcal {

  // Generate structure of channel-level arrays (SoA) layout with module mapping information
  GENERATE_SOA_LAYOUT(HGCalMappingModuleParamSoALayout,
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

  // Generate structure of channel-level arrays (SoA) layout with cell mapping information for both silicon and SiPM
  GENERATE_SOA_LAYOUT(HGCalMappingCellParamSoALayout,
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

#endif  // CondFormats_HGCalObjects_interface_HGCalMappingParameterSoA_h
