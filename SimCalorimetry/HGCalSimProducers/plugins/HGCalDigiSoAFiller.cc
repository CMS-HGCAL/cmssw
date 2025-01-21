#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "DataFormats/DetId/interface/DetId.h"
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

#include <unordered_map>

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
  void fillDetIdToIndexMaps(const hgcal::HGCalDenseIndexInfoHost &);

  //input tokens
  edm::EDGetTokenT<HGCalDigiCollection> phase1DigisCEETkn_,phase1DigisCEHTkn_,phase1DigisCEHSciTkn_;

  // config tokens
  //edm::ESGetToken<HGCalMappingCellIndexer, HGCalElectronicsMappingRcd> cellIndexToken_;
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexToken_;
  edm::ESGetToken<HGCalConfiguration, HGCalModuleConfigurationRcd> configToken_;
  edm::ESGetToken<hgcal::HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexInfoTkn_;
  
  // output tokens
  const edm::EDPutTokenT<hgcaldigi::HGCalDigiHost> digisToken_;

  //DetId to target index maps
  std::unordered_map<uint32_t, uint32_t> detId2IdxCEE_, detId2IdxCEH_, detId2IdxCEHSci_;
};

//
HGCalDigiSoAFiller::HGCalDigiSoAFiller(const edm::ParameterSet& iConfig) :
  phase1DigisCEETkn_( consumes<HGCalDigiCollection>( iConfig.getUntrackedParameter<edm::InputTag>("PhaseIDigisCEE") ) ),
  phase1DigisCEHTkn_( consumes<HGCalDigiCollection>( iConfig.getUntrackedParameter<edm::InputTag>("PhaseIDigisCEHSi") ) ),
  phase1DigisCEHSciTkn_( consumes<HGCalDigiCollection>( iConfig.getUntrackedParameter<edm::InputTag>("PhaseIDigisCEHSci") ) ),
  moduleIndexToken_(esConsumes()),
  configToken_(esConsumes()),
  denseIndexInfoTkn_(esConsumes()),
  digisToken_(produces<hgcaldigi::HGCalDigiHost>())
{
  std::cout << " [HGCalDigiSoAFiller]" << std::endl; 
}

//
void HGCalDigiSoAFiller::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) {
  std::cout << " [HGCalDigiSoAFiller] beginRun" << std::endl; 
}

//
void HGCalDigiSoAFiller::fillDetIdToIndexMaps(const hgcal::HGCalDenseIndexInfoHost &denseIndexInfo) {

  std::cout << " [HGCalDigiSoAFiller] fillDetIdToIndexMaps" << std::endl; 
  
  //skip if this has been done already
  if(detId2IdxCEE_.size()>0) return;
  
  //loop over dense indices
  auto denseIndexInfo_view = denseIndexInfo.const_view();
  int32_t ndii = denseIndexInfo_view.metadata().size();
  int32_t nunknown = 0;
  std::cout << "[HGCalDigiSoAFiller::fillDetIdToIndexMaps] fill the DetId to dense index inverse map with " << ndii << " entries" << std::endl;
  for(int32_t i=0; i<ndii; i++) {
    auto indexinfo = denseIndexInfo_view[i];
    uint32_t detIdVal = indexinfo.detid();
    DetId detId(detIdVal);
    if (detId.det() == DetId::HGCalEE) {
      detId2IdxCEE_[i] = detIdVal;
    }
    else if(detId.det() == DetId::HGCalHSi) {
      detId2IdxCEH_[i] = detIdVal;
    }
    else if(detId.det() == DetId::HGCalHSc) {
      detId2IdxCEHSci_[i] = detIdVal;
    }
    else {
      nunknown++;
    }
  }
  std::cout << "\t caught " << nunknown << " detids" << std::endl;
}

//
void HGCalDigiSoAFiller::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  std::cout << " [HGCalDigiSoAFiller] produce" << std::endl;
  
  // retrieve logical mapping and dense indexing
  const auto& moduleIndexer = iSetup.getData(moduleIndexToken_);
  //const auto& cellIndexer = iSetup.getData(cellIndexToken_);
  const auto& config = iSetup.getData(configToken_);
  const auto& denseIndexInfo = iSetup.getData(denseIndexInfoTkn_);
  fillDetIdToIndexMaps(denseIndexInfo);
    
  //book space needed for SoA DIGIs
  hgcaldigi::HGCalDigiHost digis(moduleIndexer.getMaxDataSize(), cms::alpakatools::host());

  //retrieve Phase I type of digis
  edm::Handle<HGCalDigiCollection> phase1DigisCEE,phase1DigisCEH,phase1DigisCEHSci;
  iEvent.getByToken(phase1DigisCEETkn_,phase1DigisCEE);
  iEvent.getByToken(phase1DigisCEHTkn_,phase1DigisCEH);
  iEvent.getByToken(phase1DigisCEHSciTkn_,phase1DigisCEHSci);

  std::cout << " Event digis" << std::endl;
  std::cout << phase1DigisCEE->size() << std::endl;
  std::cout << phase1DigisCEH->size() << std::endl;
  std::cout << phase1DigisCEHSci->size() << std::endl;
  
  //loop and fill in the corresponding SoA index
  //FIXME

  // put information to the event
  iEvent.emplace(digisToken_, std::move(digis));
}

// fill descriptions
void HGCalDigiSoAFiller::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.addUntracked<edm::InputTag>("PhaseIDigisCEE",    edm::InputTag("simHGCalUnsuppressedDigis","EE"));
  desc.addUntracked<edm::InputTag>("PhaseIDigisCEHSi",  edm::InputTag("simHGCalUnsuppressedDigis","HEfront"));
  desc.addUntracked<edm::InputTag>("PhaseIDigisCEHSci", edm::InputTag("simHGCalUnsuppressedDigis","HEback"));
  descriptions.addWithDefaultLabel(desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalDigiSoAFiller);
