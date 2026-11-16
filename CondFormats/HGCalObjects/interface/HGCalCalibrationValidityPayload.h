#ifndef CondFormats_HGCalObjects_HGCalCalibrationValidityPayload_h
#define CondFormats_HGCalObjects_HGCalCalibrationValidityPayload_h

#include "CondFormats/Serialization/interface/Serializable.h"
#include <string>
#include <vector>

// Arrays follow the channel order of the input module calibration.
struct HGCalCalibrationValidityModulePayload {
  std::string typecode;
  std::vector<unsigned char> valid;

  COND_SERIALIZABLE;
};

class HGCalCalibrationValidityPayload {
public:
  std::vector<HGCalCalibrationValidityModulePayload> modules;

  COND_SERIALIZABLE;
};

#endif
