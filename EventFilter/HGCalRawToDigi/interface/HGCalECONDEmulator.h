#ifndef EventFilter_HGCalRawToDigi_HGCalECONDEmulator_h
#define EventFilter_HGCalRawToDigi_HGCalECONDEmulator_h

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiCollections.h"

/**
   @class HGCalECONDEmulator
   @short performs the basic ZS and packing algorithms foreseen for the HGCAL ECON-D
 */

class HGCalECONDEmulator {
public:
  explicit HGCalECONDEmulator(const edm::ParameterSet& iConfig);

  ~HGCalECONDEmulator(){};

  typedef std::vector<HGCROCChannelDataFrameSpec> HGCROCChannelCollection_t;
  typedef std::vector<HGCROCChannelCollection_t> ERxCollection_t;

  /**
     @short runs the ECON-D algorithm
     @returns the raw data frame
   */
  std::vector<uint32_t> run(
      uint16_t bx, uint8_t orb, uint16_t l1a, std::vector<uint64_t>& enabled_channels, ERxCollection_t& eRx_inputs);

private:
  /**
     @short handles the packing o the ROC data channel each eRx at a time
     @returns the packed data raw data
   */
  std::vector<uint32_t> generateERxData(
      uint16_t bx, uint8_t orb, uint16_t l1a, std::vector<uint64_t>& enabled_channels, ERxCollection_t& eRx_inputs);

  /**
     @short steers the computation of the CRC
     @returns the CRC value
   */
  uint8_t computeCRC(const std::vector<uint32_t>&) const;

  const unsigned int header_marker_;
  bool nzs_, passthrough_;
  bool bitH_, bitT_, bitE_, bitB_, bitO_, bitS_;
};

#endif
