#ifndef CondFormats_HGCalObjects_HGCalCondSerializablePedestals_h
#define CondFormats_HGCalObjects_HGCalCondSerializablePedestals_h

#include <map>
#include <cstdint>
#include "DataFormats/Math/interface/libminifloat.h"

#include "CondFormats/Serialization/interface/Serializable.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

/**
   @short representation of a si-cell channel information read from a txt file or db
*/
struct HGCalPedestals {
  uint16_t pedestal, cm_slope, cm_offset, kappa_bxm1;
  COND_SERIALIZABLE;
};

struct HGCalFloatPedestals {
  float pedestal, cm_slope, cm_offset, kappa_bxm1;
};


/**
   @holder for the si cell channel mapping
 */
class HGCalCondSerializablePedestals {
  
public:
  
  HGCalCondSerializablePedestals() {}
  virtual ~HGCalCondSerializablePedestals() {}
  inline HGCalCondSerializablePedestals& addParameter(uint32_t id,HGCalPedestals &p) {
    if (params_.count(id) != 0)
      throw cms::Exception("HGCalCondSerializablePedestals") << "Already existing parameter with id='" << id << "'.";
    params_[id] = p;
    return *this;
  }
  inline const HGCalPedestals& parameters(uint32_t id) {
    if (params_.count(id) != 0)
      throw cms::Exception("HGCalCondSerializablePedestals") << "Failed to retrieve parameter with id='" << id << "'.";
    return params_[id];
  }
  
  inline static HGCalFloatPedestals getFloatPedestals(const HGCalPedestals&table) {
    HGCalFloatPedestals fp;
    fp.pedestal = MiniFloatConverter::float16to32(table.pedestal);
    fp.cm_slope = MiniFloatConverter::float16to32(table.cm_slope);
    fp.cm_offset = MiniFloatConverter::float16to32(table.cm_offset);
    fp.kappa_bxm1 = MiniFloatConverter::float16to32(table.kappa_bxm1);
    return fp;
  }
  
  std::map<uint32_t,HGCalPedestals> params_;
  
  COND_SERIALIZABLE;
};

#endif
