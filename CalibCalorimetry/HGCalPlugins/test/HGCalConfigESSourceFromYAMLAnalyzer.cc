/****************************************************************************
 *
 * This is a part of HGCal offline software.
 * Author:
 *   Laurent Forthomme
 *
 ****************************************************************************/

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableConfigRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableConfig.h"

class HGCalConfigESSourceFromYAMLAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalConfigESSourceFromYAMLAnalyzer(const edm::ParameterSet& iConfig)
      : tokenConds_(esConsumes<HGCalCondSerializableConfig, HGCalCondSerializableConfigRcd>(
            edm::ESInputTag(iConfig.getParameter<std::string>("label")))) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<std::string>("label", {});
    descriptions.addWithDefaultLabel(desc);
  }

private:
  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {
    // get timing calibration parameters
    if (cfgWatcher_.check(iSetup)) {
      auto conds = iSetup.getData(tokenConds_);
      size_t nmods = conds.moduleConfigs.size();
      edm::LogInfo("HGCalConfigESSourceFromYAMLAnalyzer") << "Conditions retrieved for " << nmods << " modules:\n" << conds;
      for(auto it : conds.moduleConfigs) { // loop over map module electronicsId -> HGCalModuleConfig
        HGCalModuleConfig moduleConfig(it.second);
        edm::LogInfo("HGCalConfigESSourceFromYAMLAnalyzer")
          << "  Module " << it.first << ":\n"
          << "    charMode " << moduleConfig.charMode
          << "    gain[0] " << moduleConfig.gains[0];
      }
    }
  }

  edm::ESWatcher<HGCalCondSerializableConfigRcd> cfgWatcher_;
  edm::ESGetToken<HGCalCondSerializableConfig, HGCalCondSerializableConfigRcd> tokenConds_;
};

DEFINE_FWK_MODULE(HGCalConfigESSourceFromYAMLAnalyzer);
