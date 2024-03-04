#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/HGCalDigi/interface/HGCalDigiCollections.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHostCollection.h"

#include <iostream>
#include <algorithm>

class HGCalSoATester : public edm::one::EDAnalyzer<> {

public:
  
  explicit HGCalSoATester(const edm::ParameterSet& iConfig)
    : digisToken_(consumes<HGCalElecDigiCollection>(iConfig.getParameter<edm::InputTag>("Digis"))),
      soaDigisToken_(consumes<hgcaldigi::HGCalDigiHostCollection>(iConfig.getParameter<edm::InputTag>("SoADigis")))
  {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("Digis",edm::InputTag("hgcalDigis"));
    desc.add<edm::InputTag>("SoADigis",edm::InputTag("hgcalDigis"));
    descriptions.addWithDefaultLabel(desc);
  }

private:

  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override;

  //tokens to access collections in ROOT file
  const edm::EDGetTokenT<HGCalElecDigiCollection> digisToken_;
  const edm::EDGetTokenT<hgcaldigi::HGCalDigiHostCollection> soaDigisToken_;
};


//
void HGCalSoATester::analyze(const edm::Event &iEvent, const edm::EventSetup& iSetup) {

  const auto& digis = iEvent.get(digisToken_);
  const auto& soadigis = iEvent.get(soaDigisToken_);
  auto const& soadigis_view = soadigis.const_view();

  //assert collections have the same size
  assert((int32_t)digis.size()==soadigis_view.metadata().size());

  //loop over collection of SoA digis
  for(int32_t i = 0; i < soadigis_view.metadata().size(); ++i) {

    auto vi = soadigis_view[i];

    //assert 1:1 correspondence to "classic" digi by electronics id
    HGCalElectronicsId elecId(vi.electronicsId());
    auto _elecIdMatch = [elecId](HGCROCChannelDataFrameElecSpec d){
       return d.id()==elecId;
    };
    auto it = std::find_if(digis.begin(), digis.end(), _elecIdMatch);
    assert(it!=digis.end());

    //assert values match
    assert(vi.tctp()==it->tctp());
    assert(vi.adcm1()==it->adcm1());
    assert(vi.adc()==it->adc());
    assert(vi.tot()==it->tot());
    assert(vi.toa()==it->toa());
  }
  
}


DEFINE_FWK_MODULE(HGCalSoATester);
