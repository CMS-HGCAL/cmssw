#ifndef CondFormats_HGCalObjects_HGCalNoisePayload_h
#define CondFormats_HGCalObjects_HGCalNoisePayload_h

#include "CondFormats/Serialization/interface/Serializable.h"
#include <string>
#include <vector>

// Arrays follow the channel order of the input module calibration.
struct HGCalNoiseModulePayload {
  std::string typecode;
  std::vector<float> noise;

  COND_SERIALIZABLE;
};

class HGCalNoisePayload {
public:
  std::vector<HGCalNoiseModulePayload> modules;

  COND_SERIALIZABLE;
};

#endif
