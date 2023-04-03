#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/CRC16.h"
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalRawDataPackingTools.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalECONDEmulator.h"

//
HGCalECONDEmulator::HGCalECONDEmulator(const edm::ParameterSet& iConfig)
    : header_marker_(iConfig.getParameter<unsigned int>("headerMarker")),
      nzs_(iConfig.getParameter<bool>("nzs")),
      passthrough_(iConfig.getParameter<bool>("passthrough")) {}

//
std::vector<uint32_t> HGCalECONDEmulator::run(
    uint16_t bx, uint8_t orb, uint16_t l1a, std::vector<uint64_t>& enabled_channels, ERxCollection_t& eRx_inputs) {
  //pack the eRx inputs
  auto econd_frame = generateERxData(bx, orb, l1a, enabled_channels, eRx_inputs);
  LogDebug("HGCalECONDEmulator") << econd_frame.size() << " word(s) of eRx payloads inserted.";

  //do the packet header now that event size is know and the error bits hve been set
  auto econd_header = hgcal::econd::eventPacketHeader(header_marker_,
                                                      econd_frame.size() + 1,
                                                      true,
                                                      false,
                                                      // HGCROC Event reco status across all active eRxE-B-O:
                                                      // FIXME check endianness of these two numbers
                                                      (bitH_ << 1) | bitT_,                 // HDR/TRL numbers
                                                      (bitE_ << 2) | (bitB_ << 1) | bitO_,  // Event/BX/Orbit numbers
                                                      false,
                                                      false,
                                                      0,
                                                      bx,
                                                      orb,
                                                      l1a,
                                                      bitS_,  // OR of "Stat" bits for all active eRx
                                                      0,
                                                      0);

  //last word is crc
  uint32_t crc = computeCRC(econd_header);

  //compose final event
  std::vector<uint32_t> econd_event;
  econd_event.insert(econd_event.begin(), econd_header.begin(), econd_header.end());
  econd_event.insert(econd_event.end(), econd_frame.begin(), econd_frame.end());
  econd_event.push_back(crc);
  LogDebug("HGCalECONDEmulator") << econd_event.size() << " total word s";

  return econd_event;
}

//
std::vector<uint32_t> HGCalECONDEmulator::generateERxData(
    uint16_t bx, uint8_t orb, uint16_t l1a, std::vector<uint64_t>& enabled_channels, ERxCollection_t& eRx_inputs) {
  std::vector<uint32_t> erx_data;

  for (size_t ieRx = 0; ieRx < eRx_inputs.size(); ieRx += 2) {
    HGCROCChannelCollection_t* roc[2] = {&(eRx_inputs[ieRx]), &(eRx_inputs[ieRx + 1])};

    //first two words of each half are common mode words
    uint16_t cm0 = (roc[0]->at(0)).adc();
    uint16_t cm1 = (roc[0]->at(1)).adc();
    uint16_t cm2 = (roc[1]->at(0)).adc();
    uint16_t cm3 = (roc[1]->at(1)).adc();

    for (size_t ih = 0; ih < 2; ih++) {
      uint64_t ch_status(enabled_channels[ieRx + ih]);

      //build the eRx header
      auto erxHeader =
          hgcal::econd::eRxSubPacketHeader(0, 0, false, ih == 0 ? cm0 : cm2, ih == 0 ? cm1 : cm3, ch_status);
      erx_data.insert(erx_data.end(), erxHeader.begin(), erxHeader.end());

      //add the channel data (non-common mode)
      uint8_t msb(32);
      for (size_t ich = 2; ich < roc[ih]->size(); ich++) {
        uint16_t tctp = (roc[ih]->at(ich)).tctp();
        uint16_t adc = (roc[ih]->at(ich)).adc();
        uint16_t tot = (roc[ih]->at(ich)).rawtot();
        uint16_t adcm1 = (roc[ih]->at(ich)).adcm1();
        uint16_t toa = (roc[ih]->at(ich)).toa();

        //placeholder while ZS filters are not implemented
        bool passZS = true;
        bool passZSm1 = true;
        bool passToA = true;

        //compute new words
        auto ch_data =
            hgcal::econd::addChannelData(msb, tctp, adc, tot, adcm1, toa, passZS, passZSm1, passToA, passthrough_);

        //in passthrough mode all words are 32b
        if (passthrough_) {
          erx_data.push_back(ch_data[0]);
        } else {
          //if first time just append all the new words
          if (erx_data.empty()) {
            erx_data.insert(erx_data.end(), ch_data.begin(), ch_data.end());
          }
          //if already existing and MSB=32: can copy directly all new words
          else if (msb == 32) {
            erx_data.insert(erx_data.end(), ch_data.begin(), ch_data.end());
          }
          //if already existing and MSB<32: do an or with the first words and copy the second if existing
          else {
            erx_data.back() |= ch_data[0];
            if (ch_data.size() > 1)
              erx_data.push_back(ch_data[1]);
          }
        }
      }
    }
  }

  return erx_data;
}

//
uint8_t HGCalECONDEmulator::computeCRC(const std::vector<uint32_t>& event_header) const {
  uint8_t crc = 0;  //FIXME 8-bit Bluetooth CRC
  return crc;
}
