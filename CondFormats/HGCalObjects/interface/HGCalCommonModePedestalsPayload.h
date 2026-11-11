#ifndef CondFormats_HGCalObjects_HGCalCommonModePedestalsPayload_h
#define CondFormats_HGCalObjects_HGCalCommonModePedestalsPayload_h

#include "CondFormats/Serialization/interface/Serializable.h"
#include <string>
#include <vector>

// Arrays follow the channel order of the input module calibration.
struct HGCalCommonModePedestalsModulePayload {
  std::string typecode;
  std::vector<float> cmPed;

  COND_SERIALIZABLE;
};

class HGCalCommonModePedestalsPayload {
public:
  std::vector<HGCalCommonModePedestalsModulePayload> modules;

  COND_SERIALIZABLE;
};

#endif
