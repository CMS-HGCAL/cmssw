#include <cstdio>
#include <chrono>
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"
#include "CondFormats/HGCalObjects/interface/HGCalAgeingCondsHost.h"

class HGCalAlgoBasedAgeingCondProducer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalAlgoBasedAgeingCondProducer(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void analyze(const edm::Event&, const edm::EventSetup&) override;

  edm::ESWatcher<HGCalElectronicsMappingRcd> cfgWatcher_;
  edm::ESGetToken<hgcal::HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexTkn_;
};

//
HGCalAlgoBasedAgeingCondProducer::HGCalAlgoBasedAgeingCondProducer(const edm::ParameterSet& iConfig)
    : denseIndexTkn_(esConsumes()) 
    {}

//
void HGCalAlgoBasedAgeingCondProducer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  // if the cfg didn't change there's nothing else to do
  if (!cfgWatcher_.check(iSetup))
    return;

  //test dense index token
  auto const& denseIndexInfo = iSetup.getData(denseIndexTkn_);
  printf("Retrieved %d dense index info\n", denseIndexInfo.view().metadata().size());
  int nindices = denseIndexInfo.view().metadata().size();
  std::cout << nindices << " indices retrieved from dense index info" << std::endl;
}

//
void HGCalAlgoBasedAgeingCondProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::ESInputTag>("denseIndexer", edm::ESInputTag(""))->setComment("Dense indexer SoA source");
  descriptions.addWithDefaultLabel(desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalAlgoBasedAgeingCondProducer);
