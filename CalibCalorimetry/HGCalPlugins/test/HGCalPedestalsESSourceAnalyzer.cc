#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "DataFormats/Math/interface/libminifloat.h"
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializablePedestalsRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializablePedestals.h"
#include <iomanip> // for std::setw

class HGCalPedestalsESSourceAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalPedestalsESSourceAnalyzer(const edm::ParameterSet& iConfig)
      : tokenConds_(esConsumes<HGCalCondSerializablePedestals, HGCalCondSerializablePedestalsRcd>(
            edm::ESInputTag(iConfig.getParameter<std::string>("label")))) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<std::string>("label", {});
    descriptions.addWithDefaultLabel(desc);
  }

private:
  
  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {

    // check if there are new conditions and read them
    if (!cfgWatcher_.check(iSetup)) return;
    auto conds = iSetup.getData(tokenConds_);
    size_t nconds = conds.params_.size();
    edm::LogInfo("HGCalPedestalsESSourceAnalyzer") << "Conditions retrieved:\n" << nconds;

    // print out all conditions readout
    std::cout << "   ID  eRx  ROC  Channel  isCM?  Pedestal  CM slope  CM offset  kappa(BX-1)" << std::endl;
    for(auto it : conds.params_) {

      HGCalElectronicsId id(it.first);
      bool cmflag = id.isCM();
      uint32_t eRx = (uint32_t) id.econdeRx();
      uint32_t roc = (uint32_t) eRx/2;
      uint32_t ch = id.halfrocChannel();

      HGCalFloatPedestals table = conds.getFloatPedestals(it.second);

      std::cout << std::setw(5) << std::hex << id.raw() << " " << std::setw(4) << std::dec << eRx << " "
                << std::setw(4) << roc << " " << std::setw(8) << ch << " " << std::setw(6) << cmflag << " "
                << std::setw(9) << std::setprecision(3) << table.pedestal << " " << std::setw(9) << table.cm_slope << " "
                << std::setw(10) << table.cm_offset << " " << std::setw(12) << table.kappa_bxm1 << std::endl;

    }
    
  }

  edm::ESWatcher<HGCalCondSerializablePedestalsRcd> cfgWatcher_;
  edm::ESGetToken<HGCalCondSerializablePedestals, HGCalCondSerializablePedestalsRcd> tokenConds_;
};

DEFINE_FWK_MODULE(HGCalPedestalsESSourceAnalyzer);
