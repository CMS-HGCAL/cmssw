#ifndef DataFormats_HGCalDigis_HGCalElectronicsId_h
#define DataFormats_HGCalDigis_HGCalElectronicsId_h

#include <iostream>
#include <ostream>
#include <cstdint>

/**
   @class HGCalElectronicsId
   @short wrapper for a 32b data word identifying a readout channel in the raw data
   The format is the following:
   Reserved: b'[29,31]
   z side: b'[28]
   FED ID: b'[18,27]
   Capture Block ID: b'[14,17]
   ECON-D idx: b'[10,13]
   ECON-D eRx: b'[6,9]
   1/2 ROC channel number: b'[0-5]
 */

class HGCalElectronicsId {
public:
  enum HGCalElectronicsIdMask {
    kZsideMask          = 0x1,
    kFEDIDMask          = 0x3ff,
    kCaptureBlockMask   = 0xf,
    kECONDIdxMask       = 0xf,
    kECONDeRxMask       = 0xf,
    kHalfROCChannelMask = 0x3f
  };
  enum HGCalElectronicsIdShift {
    kZsideShift          = 28,
    kFEDIDShift          = 18,
    kCaptureBlockShift   = 14,
    kECONDIdxShift       = 10,
    kECONDeRxShift       = 6,
    kHalfROCChannelShift = 0
  };
  enum HGCalElectronicsIdMask32Bit {
    kZsideMask32Bit          = (0xffffffff >> kZsideShift) << kZsideShift, // 0xf0000000
    kFEDIDMask32Bit          = (0xffffffff >> kFEDIDShift) << kFEDIDShift, // 0xfffc0000
    kCaptureBlockMask32Bit   = (0xffffffff >> kCaptureBlockShift) << kCaptureBlockShift, // 0xffffc000
    kECONDIdxMask32Bit       = (0xffffffff >> kECONDIdxShift) << kECONDIdxShift, // 0xfffffc00
    kECONDeRxMask32Bit       = (0xffffffff >> kECONDeRxShift) << kECONDeRxShift, // 0xffffffc0
    kHalfROCChannelMask32Bit = (0xffffffff >> kHalfROCChannelShift) << kHalfROCChannelShift // 0xffffffff
  };

  /**
     @short CTOR
  */
  HGCalElectronicsId() : value_(0) {}
  HGCalElectronicsId(bool zside, uint16_t fedid, uint8_t captureblock, uint8_t econdidx, uint8_t econderx, uint8_t halfrocch);
  HGCalElectronicsId(uint32_t value) : value_(value) {}
  HGCalElectronicsId(const HGCalElectronicsId& o) : value_(o.value_) {}

  /**
     @short getters
  */


  uint32_t operator()() const { return value_; }
  bool operator<(const HGCalElectronicsId& oth) const { return value_ < oth.value_; }
  bool operator==(const HGCalElectronicsId& oth) const { return value_ == oth.value_; }
  uint32_t raw() const { return value_; }
  bool zSide() const;
  uint16_t fedId() const;
  uint8_t captureBlock() const;
  uint8_t econdIdx() const;
  uint8_t econdeRx() const;
  uint8_t halfrocChannel() const;
  uint8_t rocChannel() const;
  uint8_t cmWord() const;
  uint32_t econdIdxRawId() const;
  uint32_t econdeRxRawId() const;
  bool isCM() const;
  inline uint8_t roc() const { return econdeRx()/2; }
  inline uint8_t half() const { return econdeRx()%2; }
  void print(std::ostream& out = std::cout) const {
    out << "Raw=0x" << std::hex << raw() << std::dec << std::endl
        << "\tFED-ID: " << (uint32_t)fedId() << " Capture Block: " << (uint32_t)captureBlock()
        << " ECON-D idx: " << (uint32_t)econdIdx() << " eRx: " << (uint32_t)econdeRx()
        << " 1/2 ROC ch.: " << (uint32_t)halfrocChannel()
        << " isCM=" << isCM() << " zSide=" << zSide() << std::endl;
  }

private:
  // a 32-bit word
  uint32_t value_;
};

#endif
