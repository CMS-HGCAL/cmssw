#ifndef SimCalorimetry_HGCalSimAlgos_HGCalRawDataPackingTools_h
#define SimCalorimetry_HGCalSimAlgos_HGCalRawDataPackingTools_h

#include <boost/crc.hpp>
#include "SimCalorimetry/HGCalSimAlgos/interface/SlinkTypes.h"

namespace hgcal {
  namespace econd {
    struct ERxData;
    /// pack the ROC data to the ECON-D format dependending on:
    ///   - characterization mode : (TcTp + ADC + TOT + TOA) fixed 32b
    ///   - normal mode : size and fields depend on the TcTp flags
    /// \note based on Fig. 20 of ECON-D specifications
    /// \return a vector of new words
    std::vector<uint32_t> produceERxData(
        const ERxChannelEnable&, const ERxData&, bool passZS, bool passZSm1, bool hasToA, bool char_mode);

    /// returns the words for a new eRx header
    /// \note based on Fig. 33 of ECON-D specifications
    /// \return a vector with 1 or 2 32b words
    std::vector<uint32_t> eRxSubPacketHeader(uint8_t stat,
                                             uint8_t ham,
                                             bool bitE,
                                             uint16_t common_mode0,
                                             uint16_t common_mode1,
                                             const ERxChannelEnable& channel_enable);
    std::vector<uint32_t> eRxSubPacketHeader(
        uint8_t stat, uint8_t ham, bool bitE, uint16_t common_mode0, uint16_t common_mode1, uint64_t channels_map);

    /// builds the two ECON-D header words
    /// \note based on Fig. 33 of the ECON-D specs
    /// \return a vector of size 2 with the ECON-D header
    std::vector<uint32_t> eventPacketHeader(uint16_t header,
                                            uint16_t payload,
                                            bool bitP,
                                            bool bitE,
                                            uint8_t ht,
                                            uint8_t ebo,
                                            bool bitM,
                                            bool bitT,
                                            uint8_t hamming,
                                            uint16_t bx,
                                            uint16_t l1a,
                                            uint8_t orb,
                                            bool bitS,
                                            uint8_t RR);

    //builds the final ECON-D CRC word
    uint32_t computeCRC(std::vector<uint32_t> &);
  }  // namespace econd

  namespace backend {
    /// builds the capture block header (see page 16 of "HGCAL BE DAQ firmware description")
    /// \return a vector of size 2 with the 2 32b words of the capture block header
    std::vector<uint32_t> buildCaptureBlockHeader(uint32_t bunch_crossing,
                                                  uint32_t event_counter,
                                                  uint32_t orbit_counter,
                                                  const std::vector<uint8_t>& econd_statuses);
  }  // namespace backend

}  // namespace hgcal

#endif
