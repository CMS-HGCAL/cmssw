#ifndef CondFormats_HGCalObjects_HGCalESProducerTools_h
#define CondFormats_HGCalObjects_HGCalESProducerTools_h

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include <string>
#include <sstream>    // for std::istringstream
#include <fnmatch.h>  // to match glob patterns
#include <nlohmann/json.hpp>
using json = nlohmann::ordered_json;  // ordered_json preserves key insertion order

namespace hgcal {

  // @short search first match to a given typecode key in a JSON (following insertion order)
  // allow glob patterns, e.g. 'ML-*', 'M[LH]-[A-Z]3*', etc.
  inline std::string search_modkey(const std::string& module, const json& data, const std::string& name = "") {
    if (!data.is_object() or data.empty()) {
      edm::LogError("search_modkey") << "search_modkey: '" << name
                                     << "' does not have the expected map/dict structure!";
    }
    for (auto it = data.begin(); it != data.end(); ++it) {
      int match = fnmatch(it.key().c_str(), module.c_str(), FNM_EXTMATCH);
      if (match != FNM_NOMATCH) {  // found matching key !
        edm::LogInfo("search_modkey") << "search_modkey: Matched module='\e[1m" << module << "'\e[0m to modkey='\e[1m"
                                      << it.key() << "\e[0m'";
        return it.key();  // return matching key
      }
    }
    edm::LogError("search_modkey") << "search_modkey: Could not find matching key for '\e[1m" << module << "\e[0m' in '"
                                   << name << "'! Returning first key '\e[1m" << data.begin().key() << "\e[0m'...";
    return data.begin().key();  // no matching key found in whole JSON map
  }

  // @short search first match to a given FED index in a JSON (following insertion order)
  // allow glob patterns like. '1*', '1[0-5]', etc. and
  // allow numerical ranges like '0-20', '20-40', etc.
  inline std::string search_fedkey(const int& fedid, const json& data, const std::string& name = "") {
    if (!data.is_object() or data.empty())
      edm::LogError("search_fedkey") << "search_fedkey: '" << name
                                     << "' does not have the expected map/dict structure!";
    auto it = data.begin();
    std::string matchedkey = data.begin().key();  // use first key as default
    while (it != data.end()) {
      std::string fedkey = it.key();

      // try as numerical range
      int low, high;
      char dash;
      std::istringstream iss(fedkey.c_str());
      iss >> low >> dash >> high;              // parse [integer][character][integer] pattern
      if (iss.eof() and dash == '-') {         // matches pattern
        if (low <= fedid and fedid <= high) {  // matches numerical range
          matchedkey = fedkey;
          break;
        }

        // try as glob pattern
      } else {
        const std::string sfedid = std::to_string(fedid);
        int match = fnmatch(fedkey.c_str(), sfedid.c_str(), FNM_EXTMATCH);
        if (match != FNM_NOMATCH) {  // found matching key !
          matchedkey = fedkey;
          break;
        }
      }

      ++it;
    }
    if (it == data.end())
      edm::LogError("search_fedkey") << "search_fedkey: Could not find matching key for '\e[1m" << fedid
                                     << "\e[0m' in '" << name << "'! Returning first key '\e[1m" << matchedkey
                                     << "\e[0m'...";
    else
      edm::LogInfo("search_fedkey") << "search_fedkey: Matched module='\e[1m" << fedid << "'\e[0m to fedkey='\e[1m"
                                    << matchedkey << "\e[0m'";

    return matchedkey;  // no matching key found in whole JSON map
  }

  // @short check if JSON data contains key
  inline bool check_keys(const json& data,
                         const std::string& firstkey,
                         const std::vector<std::string>& keys,
                         const std::string& fname) {
    bool iscomplete = true;
    for (auto const& key : keys) {
      if (not data[firstkey].contains(key)) {
        edm::LogWarning("checkkeys") << " JSON is missing key '" << key << "' for " << firstkey << "!"
                                     << " Please check file " << fname;
        iscomplete = false;
      }
    }
    return iscomplete;
  }

}  // namespace hgcal

#endif
