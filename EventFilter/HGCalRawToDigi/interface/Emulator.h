#ifndef EventFilter_HGCalRawToDigi_Emulator_h
#define EventFilter_HGCalRawToDigi_Emulator_h

#include <cstddef>

#include "EventFilter/HGCalRawToDigi/interface/SlinkTypes.h"

namespace hgcal::econd {
  /// Pure virtual base class for a ECON-D event emulator implementation
  class Emulator {
  public:
    explicit Emulator(size_t num_channels) : num_channels_(num_channels) {}
    virtual ~Emulator() = default;

    /// Fetch the next ECON-D event
    virtual ECONDEvent next() = 0;

  protected:
    const size_t num_channels_;
  };

  /// An empty ECON-D payloads emulator
  class EmptyEmulator : public Emulator {
  public:
    using Emulator::Emulator;

    ECONDEvent next() override {
      EventId evt_id{event_id_++, bx_id_++, orbit_id_++};
      ERxData dummy_data{.adc = {}, .adcm = {}, .toa = {}, .tot = {}, .tctp = {}, .cm0 = 0, .cm1 = 0};
      ERxEvent empty_evt = {{ERx_t{0 /*chip*/, 0 /*half*/}, dummy_data}};  // map<ERx_t, ERxData>
      return ECONDEvent{evt_id, empty_evt};
    }

  private:
    uint32_t event_id_{1}, bx_id_{2}, orbit_id_{3};
  };

  /// A "trivial" ECON-D emulator emulating non-empty ECON-D events
  class TrivialEmulator : public Emulator {
  public:
    explicit TrivialEmulator(size_t num_channels, const std::vector<unsigned int>&);

    ECONDEvent next() override;

  private:
    const std::vector<unsigned int> erx_ids_;
    uint32_t event_id_{1}, bx_id_{2}, orbit_id_{3};
  };
}  // namespace hgcal::econd

#endif
