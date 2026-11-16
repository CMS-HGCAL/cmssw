#ifndef CondFormats_HGCalObjects_HGCalCommonModeSlopePayload_h
#define CondFormats_HGCalObjects_HGCalCommonModeSlopePayload_h

#include "CondFormats/Serialization/interface/Serializable.h"
#include <string>
#include <vector>

// Arrays follow the channel order of the input module calibration.
struct HGCalCommonModeSlopeModulePayload {
  std::string typecode;
  std::vector<float> cmSlope;

  COND_SERIALIZABLE;
};

class HGCalCommonModeSlopePayload {
public:
  std::vector<HGCalCommonModeSlopeModulePayload> modules;

  COND_SERIALIZABLE;
};

#endif
