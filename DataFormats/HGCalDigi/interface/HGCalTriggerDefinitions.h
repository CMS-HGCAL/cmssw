#ifndef DataFormats_HGCalDigi_HGCalTriggerDefinitions_h
#define DataFormats_HGCalDigi_HGCalTriggerDefinitions_h

#include <cstdint>  // for uint8_t

namespace hgcal {

  namespace TDAQ_FRAME {
    constexpr uint32_t TDAQ_HEADER_POS = 32, TDAQ_HEADER_MASK = 0xffffffff, 
                       TDAQ_RESERVED_POS = 24, TDAQ_BLOCK_HEADER_MASK = 0xff,
                       TDAQ_WBX_POS = 20, TDAQ_WBX_MASK = 0xf,
                       TDAQ_BUF_STAT_POS = 16, TDAQ_BUF_STAT_MASK = 0xf,
                       TDAQ_CHANNEL_ID_POS = 8, TDAQ_CHANNEL_ID_MASK = 0xff,
                       TDAQ_PKT_LENGTH_POS = 0, TDAQ_PKT_LENGTH_MASK = 0xffff;
                      
  }  // namespace BACKEND_FRAME

}  // namespace hgcal

#endif
