#ifndef CondFormats_HGCalObjects_HGCalESProducerTools_h
#define CondFormats_HGCalObjects_HGCalESProducerTools_h

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include <string>
#include <fnmatch.h>  // to match glob patterns
#include <nlohmann/json.hpp>
using json = nlohmann::ordered_json;  // ordered_json preserves key insertion order

namespace hgcal {

  // @short search first match to a given typecode key in a JSON (following insertion order)
  //        allow glob patterns, e.g. 'ML-*', 'M[LH]-[A-Z]3*', etc.
  std::string search_modkey(const std::string& module, const json& data, const std::string& name="") {
    if (!data.is_object() or data.empty()) {
      edm::LogError("search_modkey")
         << "search_modkey: '" << name << "' does not have the expected map/dict structure!";
    }
    for (auto it = data.begin(); it != data.end(); ++it) {
      int match = fnmatch(it.key().c_str(), module.c_str(), FNM_EXTMATCH);
      if (match != FNM_NOMATCH) {  // found matching key !
        edm::LogInfo("search_modkey")
          << "search_modkey: Matched module='\e[1m" << module
          << "'\e[0m to modkey='\e[1m" << it.key() << "\e[0m'";
        return it.key();  // return matching key
      }
    }
    edm::LogError("search_modkey")
       << "search_modkey: Could not find matching key for '\e[1m" << module << "\e[0m' in '"
       << name << "'! Returning first key '\e[1m" << data.begin().key() << "\e[0m'...";
    return data.begin().key();  // no matching key found in whole JSON map
  }

}  // namespace hgcal

#endif
