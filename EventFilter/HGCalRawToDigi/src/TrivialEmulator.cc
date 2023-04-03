#include "EventFilter/HGCalRawToDigi/interface/Emulator.h"

using namespace hgcal::econd;

//
TrivialEmulator::TrivialEmulator(size_t num_channels, const std::vector<unsigned int>& erx_ids)
  : Emulator(num_channels), erx_ids_(erx_ids) {
}

//
ECONDInput TrivialEmulator::next() {
  EventId evt_id{event_id_++, bx_id_++, orbit_id_++};
  ERxInput erx;
  for (const auto& erx_id : erx_ids_) {
    ERxId_t id{erx_id /*chip*/, 0 /*half*/};
    ERxData dummy_data{.cm0 = 0,
                       .cm1 = 0,
                       .tctp = std::vector<uint8_t>(num_channels_, 3),
                       .adc = std::vector<uint16_t>(num_channels_, 0),
                       .adcm = std::vector<uint16_t>(num_channels_, 0),
                       .toa = std::vector<uint16_t>(num_channels_, 0),
                       .tot = std::vector<uint16_t>(num_channels_, 0),
                       .meta = std::vector<uint32_t>(0)
    };
    erx[id] = dummy_data;
  }

  //return result
  return ECONDInput{evt_id, erx};
}
