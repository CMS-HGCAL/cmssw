#ifndef EventFilter_HGCalRawToDigi_HGCalECONDEmulator_h
#define EventFilter_HGCalRawToDigi_HGCalECONDEmulator_h

#include <cstddef>

#include "EventFilter/HGCalRawToDigi/interface/HGCalRawDataBaseEmulator.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalECONDEmulatorParameters.h"
#include "EventFilter/HGCalRawToDigi/interface/ECONDTypes.h"

namespace hgcal::econd {

  /// A "trivial" ECON-D emulator emulating non-empty ECON-D events
  class TrivialEmulator : public ECONDEmulatorBase {
  public:
    
    using ECONDEmulatorBase::ECONDEmulatorBase;

    std::unique_ptr<ECONDInput> next() override;
    
  private:
    uint32_t event_id_{1}, bx_id_{2}, orbit_id_{3};
  };
}  // namespace hgcal::econd

#endif
