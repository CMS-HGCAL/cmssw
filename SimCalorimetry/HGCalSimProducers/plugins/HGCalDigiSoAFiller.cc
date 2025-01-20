#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "DataFormats/HGCDigi/interface/HGCDigiCollections.h"
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalRawDataDefinitions.h"

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
//#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalConfiguration.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"

class HGCalDigiSoAFiller : public edm::stream::EDProducer<> {
  /**
     @this class translates for now PhaseI type of DIGIs to SoAs used upstream
   */
public:
  explicit HGCalDigiSoAFiller(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  
private:
  void produce(edm::Event&, const edm::EventSetup&) override;
  void beginRun(edm::Run const&, edm::EventSetup const&) override;

  //input tokens
  edm::EDGetTokenT<HGCalDigiCollection> phase1DigisCEETkn_,phase1DigisCEHTkn_,phase1DigisCEHSciTkn_;

  // config tokens
  //edm::ESGetToken<HGCalMappingCellIndexer, HGCalElectronicsMappingRcd> cellIndexToken_;
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexToken_;
  edm::ESGetToken<HGCalConfiguration, HGCalModuleConfigurationRcd> configToken_;
  edm::ESGetToken<hgcal::HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexInfoTkn_;
  
  // output tokens
  const edm::EDPutTokenT<hgcaldigi::HGCalDigiHost> digisToken_;
};

//
HGCalDigiSoAFiller::HGCalDigiSoAFiller(const edm::ParameterSet& iConfig) :
  phase1DigisCEETkn_( consumes<HGCalDigiCollection>( iConfig.getParameter<edm::InputTag>("PhaseIDigisCEE") ) ),
  phase1DigisCEHTkn_( consumes<HGCalDigiCollection>( iConfig.getParameter<edm::InputTag>("PhaseIDigisCEHSi") ) ),
  phase1DigisCEHSciTkn_( consumes<HGCalDigiCollection>( iConfig.getParameter<edm::InputTag>("PhaseIDigisCEHSci") ) ),
  moduleIndexToken_(esConsumes()),
  configToken_(esConsumes()),
  denseIndexInfoTkn_(esConsumes()),
  digisToken_(produces<hgcaldigi::HGCalDigiHost>())
{}

//
void HGCalDigiSoAFiller::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) {
}

//
void HGCalDigiSoAFiller::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  // retrieve logical mapping and dense indexing
  const auto& moduleIndexer = iSetup.getData(moduleIndexToken_);
  //const auto& cellIndexer = iSetup.getData(cellIndexToken_);
  const auto& config = iSetup.getData(configToken_);
  const auto& denseIndexInfo = iSetup.getData(denseIndexInfoTkn_);
  const auto& denseIndexInfo_view = denseIndexInfo.const_view();

  //book space needed for SoA DIGIs
  hgcaldigi::HGCalDigiHost digis(moduleIndexer.getMaxDataSize(), cms::alpakatools::host());

  //retrieve Phase I type of digis
  edm::Handle<HGCalDigiCollection> phase1DigisCEE,phase1DigisCEH,phase1DigisCEHSci;
  iEvent.getByToken(phase1DigisCEETkn_,phase1DigisCEE);
  iEvent.getByToken(phase1DigisCEHTkn_,phase1DigisCEH);
  iEvent.getByToken(phase1DigisCEHSciTkn_,phase1DigisCEHSci);
  
  //loop and fill in the corresponding SoA index
  //FIXME

  // put information to the event
  iEvent.emplace(digisToken_, std::move(digis));
}

// fill descriptions
void HGCalDigiSoAFiller::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("PhaseIDigisCEE",    edm::InputTag("simHGCalUnsuppressedDigis","EE"));
  desc.add<edm::InputTag>("PhaseIDigisCEHSi",  edm::InputTag("simHGCalUnsuppressedDigis","HEfront"));
  desc.add<edm::InputTag>("PhaseIDigisCEHSci", edm::InputTag("simHGCalUnsuppressedDigis","HEback"));
  descriptions.add("hgcalDigis", desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalDigiSoAFiller);
