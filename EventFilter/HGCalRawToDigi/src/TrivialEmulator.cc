#include "EventFilter/HGCalRawToDigi/interface/Emulator.h"

namespace hgcal::econd {
  TrivialEmulator::TrivialEmulator(size_t num_channels, const std::vector<unsigned int>& erx_ids)
      : Emulator(num_channels), erx_ids_(erx_ids) {}

  ECONDEvent TrivialEmulator::next() {
    EventId evt_id{event_id_++, bx_id_++, orbit_id_++};
    ERxEvent evt;
    for (const auto& erx_id : erx_ids_) {
      ERx_t id{erx_id /*chip*/, 0 /*half*/};
      ERxData dummy_data{.adc = std::vector<uint16_t>(num_channels_, 0),
                         .adcm = std::vector<uint16_t>(num_channels_, 0),
                         .toa = std::vector<uint16_t>(num_channels_, 0),
                         .tot = std::vector<uint16_t>(num_channels_, 0),
                         .tctp = std::vector<uint8_t>(num_channels_, 3),
                         .cm0 = 0,
                         .cm1 = 0};
      evt[id] = dummy_data;
    }
    return ECONDEvent{evt_id, evt};
  }
}  // namespace hgcal::econd
