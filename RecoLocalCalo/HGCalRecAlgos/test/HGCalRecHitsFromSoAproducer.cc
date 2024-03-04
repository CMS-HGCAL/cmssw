// -*- C++ -*-
//
// Package:    RecoLocalCalo/HGCalRecHitsFromSoAproducer
// Class:      HGCalRecHitsFromSoAproducer
//
/**\class HGCalRecHitsFromSoAproducer HGCalRecHitsFromSoAproducer.cc RecoLocalCalo/HGCalRecAlgos/test/HGCalRecHitsFromSoAproducer.cc

 Description: reads SoA DIGIS and RecHits, performs basic checks and puts a classic Phase I RecHit Collection in the event translated from the SoA one

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Pedro Vieira De Castro Ferreira Da Silva
//         Created:  Tue, 04 Jul 2023 14:20:37 GMT
//
//

// system include files
#include <memory>

// user include files
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"

#include "DataFormats/HGCalDigi/interface/HGCalDigiHostCollection.h"
#include "DataFormats/HGCalRecHit/interface/HGCalRecHitHostCollection.h"
#include "DataFormats/HGCRecHit/interface/HGCRecHitCollections.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiCellChannelInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalElectronicsMappingTools.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include <iostream>
#include <algorithm>

//
// class declaration
//

class HGCalRecHitsFromSoAproducer : public edm::stream::EDProducer<> {
public:
  explicit HGCalRecHitsFromSoAproducer(const edm::ParameterSet&);
  ~HGCalRecHitsFromSoAproducer() override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void beginStream(edm::StreamID) override;
  void produce(edm::Event&, const edm::EventSetup&) override;
  void endStream() override;

  void beginRun(edm::Run const&, edm::EventSetup const&) override;
  
  //tokens to access collections in ROOT file
  const edm::EDGetTokenT<hgcaldigi::HGCalDigiHostCollection> digisToken_;
  const edm::EDGetTokenT<hgcalrechit::HGCalRecHitHostCollection> rechitsToken_;
  const edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
  const edm::ESGetToken<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd> siModuleInfoToken_;

  //electronics ID to det ID map
  std::map<uint32_t,uint32_t> ele2detid_;
};

//
HGCalRecHitsFromSoAproducer::HGCalRecHitsFromSoAproducer(const edm::ParameterSet& iConfig)
  : digisToken_(consumes<hgcaldigi::HGCalDigiHostCollection>(iConfig.getParameter<edm::InputTag>("Digis"))),
    rechitsToken_(consumes<hgcalrechit::HGCalRecHitHostCollection>(iConfig.getParameter<edm::InputTag>("RecHits"))),
    moduleInfoToken_(esConsumes<HGCalCondSerializableModuleInfo,HGCalCondSerializableModuleInfoRcd,edm::Transition::BeginRun>(iConfig.getParameter<edm::ESInputTag>("ModuleInfo"))),
    siModuleInfoToken_(esConsumes<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd,edm::Transition::BeginRun>(iConfig.getParameter<edm::ESInputTag>("SiModuleInfo")))
{
  produces<HGCRecHitCollection>();
}

//
HGCalRecHitsFromSoAproducer::~HGCalRecHitsFromSoAproducer() {
}


//
void HGCalRecHitsFromSoAproducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  
  auto hgcRecHits = std::make_unique<HGCRecHitCollection>();

  //retrieve digis and rechits
  const auto& digis = iEvent.get(digisToken_);
  auto const& digis_view = digis.const_view();
  int32_t ndigis=digis_view.metadata().size();
  
  const auto& rechits = iEvent.get(rechitsToken_);
  auto const& rechits_view = rechits.const_view();
  int32_t nhits=rechits_view.metadata().size();

  //produce the Phase I type of RecHits if
  //  ElectronicsId to DetId mapping has an entry
  //  RecHit
  uint32_t ndetidFound(0);
  for(int32_t i = 0; i < rechits_view.metadata().size(); ++i) {

    auto rh = rechits_view[i];
    uint32_t eleid=rh.detid(); //in practice they were filled with electronics id
    if(ele2detid_.count(eleid)!=1) continue;
    ndetidFound +=1;

    DetId detid(ele2detid_[eleid]);
    hgcRecHits->push_back( HGCRecHit(detid,rh.energy(),rh.time(),rh.flags()) );    
  }
  
  //report
  LogDebug("HGCalRecHitsFromSoAproducer")
      << "ndigis=" << ndigis << " nhits=" << nhits << std::endl
      << "can assign " << ndetidFound << " detIds to DIGIS from mapping" << std::endl;

  //put to the event
  hgcRecHits->sort();
  iEvent.put(std::move(hgcRecHits));
}

//
void HGCalRecHitsFromSoAproducer::beginStream(edm::StreamID) {
  // please remove this method if not needed
}

//
void HGCalRecHitsFromSoAproducer::endStream() {
  // please remove this method if not needed
}

//
void HGCalRecHitsFromSoAproducer::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup)
{
  //get module and silicon cell mapping
  auto moduleInfo = iSetup.getData(moduleInfoToken_);
  auto siCellInfo = iSetup.getData(siModuleInfoToken_);
  ele2detid_=hgcal::mapSiGeoToElectronics(moduleInfo,siCellInfo,false);
  edm::LogInfo("HGCalRecHitsFromSoAproducer") << "Retrieved ele->det mapping with " << ele2detid_.size() << " entries from event setup" << std::endl;

}


//
void HGCalRecHitsFromSoAproducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("Digis",edm::InputTag("hgcalDigis"));
    desc.add<edm::InputTag>("RecHits",edm::InputTag("hgcalRecHit"));
    desc.add<edm::ESInputTag>("ModuleInfo",edm::ESInputTag(""));
    desc.add<edm::ESInputTag>("SiModuleInfo",edm::ESInputTag(""));
    descriptions.addWithDefaultLabel(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(HGCalRecHitsFromSoAproducer);
