#ifndef DataFormats_HGCalDigi_interface_HGCalECONTPacketInfoSoA_h
#define DataFormats_HGCalDigi_interface_HGCalECONTPacketInfoSoA_h

#include <cstdint>  // for uint8_t

#include <Eigen/Core>

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"

namespace hgcaldigi {


  // generate structure of arrays (SoA) layout with Digi dataformat
  GENERATE_SOA_LAYOUT(HGCalECONTPacketInfoSoALayout,
                      // 1: Wrong S-Link header marker
                      // 2: ... to be added
		      SOA_COLUMN(uint8_t, exception),
                      // Location
                      // If exception found before ECON-D, this would be 0
                      // Otherwise the 64b index of ECON-D header
                      SOA_COLUMN(uint32_t, location))
                      // Payload length
                      // If exception found before ECON-T, this would be 0
                      // Otherwise the payload length of the ECON-T
  using HGCalECONTPacketInfoSoA = HGCalECONTPacketInfoSoALayout<>;
}  // namespace hgcaldigi

#endif
