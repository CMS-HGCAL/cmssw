// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "CommonTools/Utils/interface/StringCutObjectSelector.h"

#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "DataFormats/HGCalDigi/interface/HGCalTestSystemMetadata.h"

#include <iostream>

//
// class declaration
//

class HGCalMetaDataTableProducer : public edm::stream::EDProducer<> {
public:
  explicit HGCalMetaDataTableProducer(const edm::ParameterSet&);
  ~HGCalMetaDataTableProducer() override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void beginStream(edm::StreamID) override;
  void produce(edm::Event&, const edm::EventSetup&) override;
  void endStream() override;

  // ----------member data ---------------------------

  const edm::EDGetTokenT<HGCalTestSystemMetaData> metadataToken_;

};

//
// constructors and destructor
//
HGCalMetaDataTableProducer::HGCalMetaDataTableProducer(const edm::ParameterSet& iConfig)
  : metadataToken_(consumes<HGCalTestSystemMetaData>(iConfig.getParameter<edm::InputTag>("MetaData")))
{
  produces<nanoaod::FlatTable>("hgcMetadata");

}

HGCalMetaDataTableProducer::~HGCalMetaDataTableProducer() {
  // do anything here that needs to be done at destruction time
  // (e.g. close files, deallocate resources etc.)
}

//
// member functions
//

// ------------ method called to produce the data  ------------

void HGCalMetaDataTableProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;

  const auto& info = iEvent.get(metadataToken_);  
  auto out = std::make_unique<nanoaod::FlatTable>(1,"hgcMetadata",true);

  out->addColumnValue<uint32_t>("trigType", info.trigType_, "Trig Type");
  out->addColumnValue<uint32_t>("trigSubType", info.trigSubType_, "Trig Sub-Type");
  out->addColumnValue<uint32_t>("trigTime", info.trigTime_, "Trig Time");
  out->addColumnValue<uint32_t>("trigWidth", info.trigWidth_, "Trig Width");
  out->addColumnValue<uint32_t>("injcalib", info.injcalib_, "injcalib");
  out->addColumnValue<uint8_t>("injgain", int(info.injgain_), "injgain");

  iEvent.put(std::move(out), "hgcMetadata");

}

// ------------ method called once each stream before processing any runs, lumis or events  ------------
void HGCalMetaDataTableProducer::beginStream(edm::StreamID) {}

// ------------ method called once each stream after processing all runs, lumis and events  ------------
void HGCalMetaDataTableProducer::endStream() {}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void HGCalMetaDataTableProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("MetaData",edm::InputTag("hgcalEmulatedSlinkRawData","hgcalMetaData"));
  descriptions.addWithDefaultLabel(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(HGCalMetaDataTableProducer);
