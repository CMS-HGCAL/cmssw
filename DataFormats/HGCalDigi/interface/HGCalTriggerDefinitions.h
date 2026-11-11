#ifndef DataFormats_HGCalDigi_HGCalTriggerDefinitions_h
#define DataFormats_HGCalDigi_HGCalTriggerDefinitions_h

#include <cstdint>  // for uint8_t
#include <map> // std::map

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

  namespace econt {
      /*
    * @short Basic "type" enum, originally defined
    * in EventFilter/HGCalRawToDigi/interface/TPGFEDataformat.hh
    * (where most of this namespace resides), but moved here to avoid
    * the possibility of a dependency cycle. Keeping the old namespace,
    * we will likely want to change this! -Jan
    * Data Formats by Paul and Indra.
    */
    enum Type {
        //select : 0 = Threshold Sum (TS), 1 = Super Trigger Cell (STC), 2 = Best Choice (BC), 3 = Repeater, 4 = Autoencoder (AE) Settings of [5,7] revert to the Repeater algorithm
        //stc_type : 0 = STC4B(5E+4M), 1 = STC16(5E+4M), 2 = CTC4A(4E+3M), 3 = STC4A(4E+3M), 4 = CTC4B(5E+3M) // Jan: Based on below & decodedE(), CTC4B is 5E+4M. Double-check this?
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

    inline constexpr uint8_t SELECT_STC = 1;

    // ECON-T `select` -> output Type. STC (1) is absent on purpose:
    // its a special case where Type depends on stcType, see stcType2Type below.
    inline const std::map<uint8_t, Type> select2Type{
      {0, TS},
      {2, BestC},
      {3, RA},
      {4, AE}
    };

    // ECON-T `stcType` -> output Type
    inline const std::map<uint8_t, Type> stcType2Type{
      {0, STC4B},
      {1, STC16},
      {2, CTC4A},
      {3, STC4A},
      {4, CTC4B}
    };

    inline constexpr uint32_t DEFAULT_MAX_TC = 48;
    // Number of e-links -> (max) number of  TCs
    inline const std::map<uint8_t, uint32_t> NofElinks2NofTCs{
      {1,   1},
      {2,   4},
      {3,   6},
      {4,   9},
      {5,  14},
      {6,  18},
      {7,  23},
      {8,  28},
      {9,  32},
      {10, 37},
      {11, 41},
      {12, 46},
      {13, DEFAULT_MAX_TC}
    };

    inline const std::map<uint8_t, uint8_t> DEFAULT_MAX_STC{
      {0, 12},
      {1,  3},
      {2, 12},
      {3, 12},
      {4, 12}
    };
    // stcType -> { eportTxNumen -> max number of STCs }.
    // Each inner entry applies from its key upward, until the next key
    // (so the last entry covers all larger eportTxNumen).
    inline const std::map<uint8_t, std::map<uint8_t, uint32_t>> stcType2MaxSTCs{
      {
        0, {
          {1,  2},
          {2,  5},
          {3,  8},
          {4, 11},
          {5, DEFAULT_MAX_STC.at(0)} // default
        }
      }, // STC4B
      {
        1, {
          {1, 2},
          {2, DEFAULT_MAX_STC.at(1)} // default
        }
      }, // STC16
      {
        2, {
          {1,  4},
          {2,  8},
          {3, DEFAULT_MAX_STC.at(2)} // default
        }
      }, // CTC4A
      {
        3, {
          {1,  3},
          {2,  6},
          {3, 10},
          {4, DEFAULT_MAX_STC.at(3)} // default
        }
      }, // STC4A
      {
        4, {
          {1,  3},
          {2,  6},
          {3, 10},
          {4, DEFAULT_MAX_STC.at(4)} // default
        }
      } // CTC4B
    };


    /*
    * Some stuff for getExpEcontHeader, to avoid "magic numbers" in HGCalTriggerConfiguration.hh .
    * Having maps w/ a single value is a bit silly, but useful if we ever need to expand this. - Jan
    */

    // The ECON-T header is 5 bits wide for TS and 4 bits for all other types.
    // maxMSB: expected header value when BXSLink == 1 (all bits set).
    inline constexpr uint16_t DEFAULT_MAX_MSB = 15;  // 4-bit header
    inline const std::map<Type, uint16_t> maxMSB{
      {TS, 31}  // 5-bit header
    };

    // modEcontHeader: modulus of the expected header for BXSLink > 1, i.e. (BXSLink - 1) % mod.
    inline constexpr uint16_t DEFAULT_MOD_ECONT_HEADER = 8;  // 4-bit header
    inline const std::map<Type, uint16_t> modEcontHeader{
      {TS, 16}  // 5-bit header
    };

  } // namespace econt

}  // namespace hgcal
#endif
