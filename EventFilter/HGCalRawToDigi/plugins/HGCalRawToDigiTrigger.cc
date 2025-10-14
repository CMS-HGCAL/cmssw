#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "DataFormats/FEDRawData/interface/RawDataBuffer.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalECONDPacketInfoHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalFEDPacketInfoHost.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfiguration.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiTriggerHost.h"

#include "EventFilter/HGCalRawToDigi/interface/HGCalUnpackerTrigger.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexerTrigger.h"



class HGCalRawToDigiTrigger : public edm::stream::EDProducer<> {
  // TODO: this should also probably be split between DAQ and Trigger, and two producers should be created in the python config

public:
  explicit HGCalRawToDigiTrigger(const edm::ParameterSet&);

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void produce(edm::Event&, const edm::EventSetup&) override;
  void beginRun(edm::Run const&, edm::EventSetup const&) override;

  // input tokens
  const edm::EDGetTokenT<RawDataBuffer> fedRawTriggerToken_;

  // output tokens
  const edm::EDPutTokenT<hgcaldigi::HGCalDigiTriggerHost> digisTriggerToken_;

  // config tokens and objects
  edm::ESGetToken<HGCalMappingModuleIndexerTrigger, HGCalElectronicsMappingRcd> moduleIndexToken_;
  edm::ESGetToken<HGCalTriggerConfiguration, HGCalModuleConfigurationRcd> configToken_;
  HGCalUnpackerTrigger unpacker_trigger_;
};

HGCalRawToDigiTrigger::HGCalRawToDigiTrigger(const edm::ParameterSet& iConfig)
    : fedRawTriggerToken_(consumes<RawDataBuffer>(iConfig.getParameter<edm::InputTag>("src"))),
      digisTriggerToken_(produces<hgcaldigi::HGCalDigiTriggerHost>()),
      moduleIndexToken_(esConsumes()),
      configToken_(esConsumes()) {}

void HGCalRawToDigiTrigger::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) {
}

void HGCalRawToDigiTrigger::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  const auto& moduleIndexer = iSetup.getData(moduleIndexToken_);
  //const auto& cellIndexer = iSetup.getData(cellIndexToken_);
  const auto& config = iSetup.getData(configToken_);
  std::cout << "HGCalRawToDigiTrigger::produce - with module indexer " << moduleIndexer.fedReadoutSequences().size() << std::endl;
  
  hgcaldigi::HGCalDigiTriggerHost digisTrigger(moduleIndexer.maxDataSize(), cms::alpakatools::host());
  const auto& fedBuffer = iEvent.get(fedRawTriggerToken_);
  for (unsigned fedId = 0; fedId < moduleIndexer.fedCount(); ++fedId) {
    const auto& frs = moduleIndexer.fedReadoutSequences()[fedId];
    std::cout << frs.readoutTypes_.size() << " " << frs.id << std::endl;
    if (frs.readoutTypes_.empty()) {
      continue;
    }
    const auto& fed_data = fedBuffer.fragmentData(fedId);
    if (fed_data.size() == 0){
      continue;
    }
    std::cout << "HGCalRawToDigiTrigger::produce - got a fed_data fragment size=" << fed_data.size() << std::endl; 
    unpacker_trigger_.parseFEDData(fedId,fed_data,config,moduleIndexer,digisTrigger);
  }


  // put information to the event
  iEvent.emplace(digisTriggerToken_, std::move(digisTrigger));
}

// fill descriptions
void HGCalRawToDigiTrigger::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("src", edm::InputTag("rawDataCollector"));
  descriptions.add("hgcalDigisTrigger", desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalRawToDigiTrigger);
