/****************************************************************************
 *
 * This is a part of HGCal offline software.
 * Authors:
 *   Laurent Forthomme
 *
 ****************************************************************************/

#ifndef CondFormats_HGCalObjects_HGCalCondSerializableConfig_h
#define CondFormats_HGCalObjects_HGCalCondSerializableConfig_h

#include <string>
#include <map>
#include <vector>

#include "CondFormats/Serialization/interface/Serializable.h"
//#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"

///**
//   @short global Module settings read from YAML, same for all ROCs
//*/
typedef std::map<uint32_t,uint32_t> HGCALROCConfig; // ID -> gain
struct HGCalModuleConfig {
  bool charMode;         // characterization/readout mode
  uint8_t gain, injgain; // gain settings
  uint32_t injcalib;     // injected charge
  HGCALROCConfig gains;  // map between ROC ID -> parameters
  COND_SERIALIZABLE;
};

class HGCalCondSerializableConfig {
public:
  std::map<uint32_t,HGCalModuleConfig> moduleConfigs;

  HGCalCondSerializableConfig();
  ~HGCalCondSerializableConfig();

  std::vector<std::string> keys() const;
  const std::vector<int>& parameters(const std::string&) const;

  HGCalCondSerializableConfig& addParameter(const std::string&, const std::vector<int>&);
  inline HGCalCondSerializableConfig& addParameter(const std::string& key, int value) {
    return addParameter(key, std::vector<int>{value});
  }

  friend std::ostream& operator<<(std::ostream&, const HGCalCondSerializableConfig&);

private:
  std::map<std::string, std::vector<int> > params_;

  COND_SERIALIZABLE;
};

#endif
