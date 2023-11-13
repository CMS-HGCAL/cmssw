/****************************************************************************
 *
 * This is a part of HGCal offline software.
 * Authors:
 *   Laurent Forthomme
 *
 ****************************************************************************/

#include <algorithm>

#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableConfig.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

HGCalCondSerializableConfig::HGCalCondSerializableConfig() {}

HGCalCondSerializableConfig::~HGCalCondSerializableConfig() {}

std::vector<std::string> HGCalCondSerializableConfig::keys() const {
  std::vector<std::string> keys;
  std::transform(params_.begin(), params_.end(), std::back_inserter(keys), [](const auto& pair) { return pair.first; });
  std::sort(keys.begin(), keys.end());
  return keys;
}

const std::vector<int>& HGCalCondSerializableConfig::parameters(const std::string& key) const {
  if (params_.count(key) == 0)
    throw cms::Exception("HGCalCondSerializableConfig") << "Failed to retrieve a parameter with key='" << key << "'.";
  return params_.at(key);
}

HGCalCondSerializableConfig& HGCalCondSerializableConfig::addParameter(const std::string& key, const std::vector<int>& values) {
  if (params_.count(key) != 0)
    edm::LogWarning("HGCalCondSerializableConfig")
        << "Parameter with key='" << key << "' was already present in collection.";
  params_[key] = values;
  return *this;
}

std::ostream& operator<<(std::ostream& os, const HGCalCondSerializableConfig& cond) {
  os << "HGCalCondSerializableConfig{";
  std::string sep1;
  for (const auto& key : cond.keys()) {
    os << sep1 << key << ": {";
    std::string sep2;
    for (const auto& param : cond.parameters(key))
      os << sep2 << param, sep2 = ", ";
    os << "}";
    sep1 = ", ";
  }
  return os << "}";
}
