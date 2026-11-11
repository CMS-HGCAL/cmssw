#ifndef CondFormats_HGCalObjects_HGCalADCPedestalsPayload_h
#define CondFormats_HGCalObjects_HGCalADCPedestalsPayload_h

#include "CondFormats/Serialization/interface/Serializable.h"
#include <string>
#include <vector>

// Arrays follow the channel order of the input module calibration.
struct HGCalADCPedestalsModulePayload {
  std::string typecode;
  std::vector<float> adcPed;

  COND_SERIALIZABLE;
};

class HGCalADCPedestalsPayload {
public:
  std::vector<HGCalADCPedestalsModulePayload> modules;

  COND_SERIALIZABLE;
};

#endif
