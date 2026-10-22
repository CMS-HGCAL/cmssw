#ifndef CondFormats_HGCalObjects_HGCalCalibrationPayload_h
#define CondFormats_HGCalObjects_HGCalCalibrationPayload_h

#include "CondFormats/Serialization/interface/Serializable.h"

#include <string>
#include <vector>

/**
 * Channel-level calibration constants belonging to one HGCal module
 * typecode.
 *
 * TOA correction vectors are stored in flattened form:
 *   toaCTDC.size() = number of channels * 32
 *   toaFTDC.size() = number of channels * 8
 *   toaTW.size()   = number of channels * 3
 */
struct HGCalCalibrationModulePayload {
  static constexpr unsigned int kCTDCSize = 32;
  static constexpr unsigned int kFTDCSize = 8;
  static constexpr unsigned int kTWSize = 3;

  std::string typecode;

  std::vector<int> channel;

  std::vector<float> adcPed;
  std::vector<float> noise;
  std::vector<float> cmPed;
  std::vector<float> cmSlope;

  std::vector<float> bxm1Slope;
  std::vector<float> bxm1Ped;

  std::vector<float> totToADC;
  std::vector<float> totPed;
  std::vector<float> totLin;
  std::vector<float> totP0;
  std::vector<float> totP1;
  std::vector<float> totP2;

  std::vector<float> toaCTDC;
  std::vector<float> toaFTDC;
  std::vector<float> toaTW;

  std::vector<float> mipsScale;
  std::vector<unsigned char> valid;

  COND_SERIALIZABLE;
};

/**
 * Persistent HGCal calibration conditions payload.
 *
 * This object contains typed C++ calibration data. It does not contain
 * JSON text.
 */
class HGCalCalibrationPayload {
public:
  HGCalCalibrationPayload() = default;

  std::vector<HGCalCalibrationModulePayload> modules;

  COND_SERIALIZABLE;
};

#endif
