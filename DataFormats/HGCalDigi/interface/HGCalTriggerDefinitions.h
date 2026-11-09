#ifndef DataFormats_HGCalDigi_HGCalTriggerDefinitions_h
#define DataFormats_HGCalDigi_HGCalTriggerDefinitions_h

#include <cstdint>  // for uint8_t

namespace hgcal {

  namespace TDAQ_FRAME {
    constexpr uint32_t TDAQ_HEADER_POS = 32, TDAQ_HEADER_MASK = 0xffffffff,
                       TDAQ_ELINK_POS = 32, TDAQ_ELINK_MASK = 0xffffffff,
                       TDAQ_ELINK_OFFSET = 8, TDAQ_PAIRS_NELINKS = 14,
                       TDAQ_RESERVED_POS = 24, TDAQ_BLOCK_HEADER_MASK = 0xff,
                       TDAQ_WBX_POS = 20, TDAQ_WBX_MASK = 0xf,
                       TDAQ_BUF_STAT_POS = 16, TDAQ_BUF_STAT_MASK = 0xf,
                       TDAQ_CHANNEL_ID_POS = 8, TDAQ_CHANNEL_ID_MASK = 0xff,
                       TDAQ_PKT_LENGTH_POS = 0, TDAQ_PKT_LENGTH_MASK = 0xffff;

  }  // namespace BACKEND_FRAME

  namespace ECONT_FRAME {
    constexpr uint32_t HEADER_POS = 28, BC_MAX_TCADDR = 47,
                       BC_LO_TCADDR_MASK =  0x3f  ;
  }  // namespace ECONT_FRAME

}  // namespace hgcal

/*
 * @short Basic "type" enum, originally defined
 * in EventFilter/HGCalRawToDigi/interface/TPGFEDataformat.hh
 * (where most of this namespace resides), but moved here to avoid
 * the possibility of a dependency cycle. Keeping the old namespace,
 * we will likely want to change this! -Jan
 * Data Formats by Paul and Indra.
*/
namespace TPGFEDataformat {
  enum Type {
      //select : 0 = Threshold Sum (TS), 1 = Super Trigger Cell (STC), 2 = Best Choice (BC), 3 = Repeater, 4 = Autoencoder (AE) Settings of [5,7] revert to the Repeater algorithm
      //stc_type : 0 = STC4B(5E+4M), 1 = STC16(5E+4M), 2 = CTC4A(4E+3M), 3 = STC4A(4E+3M), 4 = CTC4B(5E+3M)
      BestC, // TC energy format = 4E3M
      STC4A, // 4E3M
      STC4B, // 5E4M
      STC16, // 5E4M
      CTC4A, // 4E3M
      CTC4B, // 5E4M
      TS,    // 4E3M, placeholder
      RA,    // 4E3M, Repeater algorithm is intended for testing and debugging of ECONT
      AE,    // blocks of 16b, uses convolutional neural network
      Unknown
  };
} // namespace TPGFEDataformat


#endif
