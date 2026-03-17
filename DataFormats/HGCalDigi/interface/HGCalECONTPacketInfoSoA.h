#ifndef DataFormats_HGCalDigi_interface_HGCalECONTPacketInfoSoA_h
#define DataFormats_HGCalDigi_interface_HGCalECONTPacketInfoSoA_h

#include <cstdint>  // for uint8_t

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"

namespace hgcaldigi {

  // generate structure of arrays (SoA) layout with Digi dataformat
  GENERATE_SOA_LAYOUT(HGCalECONTPacketInfoSoALayout,
                      // Exception flag
                      // 0: Normal
                      // 1: Stage1IO conversion exception while unpacking ECON-T
                      // 2: ... to be added
                      SOA_COLUMN(uint8_t, exception),
                      // Location
                      // If exception found before ECON-T, this would be 0
                      // Otherwise the 64b index of ECON-T payload start
                      SOA_COLUMN(uint32_t, location),
                      // Payload length
                      // If exception found before ECON-T, this would be 0
                      // Otherwise the payload length of the ECON-T
                      SOA_COLUMN(uint16_t, payloadLength))
  using HGCalECONTPacketInfoSoA = HGCalECONTPacketInfoSoALayout<>;
}  // namespace hgcaldigi

#endif
