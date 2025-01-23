#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/HGCDigi/interface/HGCDigiCollections.h" //This is temporary, we would have to update it to the one below.
//#include "DataFormats/HGCalDigi/interface/HGCalDigiCollections.h"
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

#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"

#include <unordered_map>

class HGCalDigiSoAFiller : public edm::stream::EDProducer<> {
  /**
     @this class translates for now PhaseI type of DIGIs to SoAs used upstream
   */
public:
  explicit HGCalDigiSoAFiller(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  
private:
  void beginStream(edm::StreamID) override {}
  void produce(edm::Event&, const edm::EventSetup&) override;
  void endStream() override {}
  void beginRun(edm::Run const&, edm::EventSetup const&) override;

  /**
     @short this function takes care of building the map: DetId -> DenseIndex
     PhaseI Digis are ordered by DetId so they need to be mapped to the dense index in the SoA     
   */
  void fillDetIdToIndexMaps(const hgcal::HGCalDenseIndexInfoHost &, const CaloGeometry &);
  void analyzeDigis(edm::Handle<HGCalDigiCollection> &digiColl, const std::unordered_map<uint32_t, uint32_t> detmap, hgcaldigi::HGCalDigiHost& digis);
  //input tokens
  edm::EDGetTokenT<HGCalDigiCollection> phase1DigisCEETkn_,phase1DigisCEHTkn_,phase1DigisCEHSciTkn_;

  // config tokens
  //edm::ESGetToken<HGCalMappingCellIndexer, HGCalElectronicsMappingRcd> cellIndexToken_;
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexToken_;
  //edm::ESGetToken<HGCalConfiguration, HGCalModuleConfigurationRcd> configToken_;
  edm::ESGetToken<hgcal::HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexInfoTkn_;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeomToken_;
  
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
  //configToken_(esConsumes()),
  denseIndexInfoTkn_(esConsumes<edm::Transition::BeginRun>()),
  caloGeomToken_(esConsumes<edm::Transition::BeginRun>()),
  digisToken_(produces<hgcaldigi::HGCalDigiHost>())
{
}

//
void HGCalDigiSoAFiller::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) {
  const auto& denseIndexInfo = iSetup.getData(denseIndexInfoTkn_);
  auto const& geo = iSetup.getData(caloGeomToken_);
  fillDetIdToIndexMaps(denseIndexInfo, geo);
}

//
void HGCalDigiSoAFiller::fillDetIdToIndexMaps(const hgcal::HGCalDenseIndexInfoHost &denseIndexInfo, const CaloGeometry &geo) {
    
  //loop over dense indices
  auto denseIndexInfo_view = denseIndexInfo.const_view();
  int32_t ndii = denseIndexInfo_view.metadata().size();
  int32_t nunknown = 0;
  int32_t ninvalid = 0;
  for(int32_t i=0; i<ndii; i++) {

    auto indexinfo = denseIndexInfo_view[i];
    uint32_t detIdVal = indexinfo.detid();
    if(detIdVal==0) continue;
    
    //rebuild the det id and check the validity in the geometry
    DetId::Detector det = DetId::HGCalEE;
    try {
      DetId detId(detIdVal);
      det = detId.det();
      //int subdet = ForwardSubdetector::ForwardEmpty;
      //const HGCalGeometry* hgcal_geom = static_cast<const HGCalGeometry*>(geo.getSubdetectorGeometry(det, subdet));
      //if(!hgcal_geom->valid(detId))
      //  throw cms::Exception("HGCalDigiSoAFiller :: invalid DetId") << detIdVal;
    }catch(cms::Exception &e) {
      ninvalid++;
      continue;
    }

    //assign in the appropriate map
    if (det == DetId::HGCalEE) {
      detId2IdxCEE_[i] = detIdVal;
    }
    else if(det == DetId::HGCalHSi) {
      detId2IdxCEH_[i] = detIdVal;
    }
    else if(det == DetId::HGCalHSc) {
      detId2IdxCEHSci_[i] = detIdVal;
    }
    else {
      nunknown++;
    }
  }

  //report result of the mapping
  size_t nvalid(detId2IdxCEE_.size()+detId2IdxCEH_.size()+detId2IdxCEHSci_.size());
  std::cout << "DetId count after building inverse mapping " << std::endl
            << "| Type        | Counts |" << std::endl
            << "| ----------- | -------- |" << std::endl
            << "| unknown     | " << nunknown << " | " << std::endl
            << "| invalid     | " << ninvalid << " | " << std::endl
            << "| CE-E        | " << detId2IdxCEE_.size() << " | " << std::endl
            << "| CE-HSi      | " << detId2IdxCEH_.size() << " | " << std::endl
            << "| CE-HSci     | " << detId2IdxCEHSci_.size() << " | " << std::endl
            << "| ----------- | -------- |" << std::endl
            << "| Total valid | " << nvalid << " | " << std::endl
            << "| Total idxs  | " << ndii << " | " << std::endl
            << "| ----------- | -------- |" << std::endl;
}
//
void HGCalDigiSoAFiller::analyzeDigis(edm::Handle<HGCalDigiCollection> &digiColl, const std::unordered_map<uint32_t, uint32_t> detmap, hgcaldigi::HGCalDigiHost& digis) {
  const int itSample(2); //in-time sample
  for(auto &hit : *digiColl)
    {
      if(hit.size()==0) continue;

      //uint32_t detmap_key(hit.rawId()); //for HGCal
      uint32_t detmap_key(hit.id()); //for HGC check the dataformat you put as a header
      if (detmap.count(detmap_key) == 0) {
        continue;
      }
      uint32_t rawData(hit.sample(itSample).data() );
      bool isTOA( hit.sample(itSample).getToAValid() );
      bool isTDC( hit.sample(itSample).mode() );
      bool isBusy( isTDC && rawData==0 );
      uint32_t tctb = 0;
      if (isBusy) {
        tctb = 1;
      }
      else if (isTDC) {
        tctb = 2;
      }

      uint32_t rawDatabxm1(hit.sample(itSample-1).data() );
      //uint32_t denseIdx(detmap[detmap_key]);
      uint32_t denseIdx = detmap.at(detmap_key);
      //TCTB and ACD-1 values
      digis.view()[denseIdx].tctp() = tctb;
      digis.view()[denseIdx].adcm1() = rawDatabxm1;

      //ADC value
      if (isTDC) {
        digis.view()[denseIdx].adc() = 0.;
        digis.view()[denseIdx].tot() = rawData;
      }
      else {
        digis.view()[denseIdx].adc() = rawData;
        digis.view()[denseIdx].tot() = 0.;
      }

      if (isTOA) {
        digis.view()[denseIdx].toa() =  hit.sample(itSample).toa();
      }
      else digis.view()[denseIdx].toa() = 0.;

      //digis.view()[denseIdx].cm() = cmSum;
      digis.view()[denseIdx].cm() = 0; // we do not simulate it
      digis.view()[denseIdx].flags() = 0;
    }

}


//
void HGCalDigiSoAFiller::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  std::cout << " [HGCalDigiSoAFiller] produce" << std::endl;
  
  // retrieve logical mapping and dense indexing
  const auto& moduleIndexer = iSetup.getData(moduleIndexToken_);
  //const auto& cellIndexer = iSetup.getData(cellIndexToken_);
  //const auto& config = iSetup.getData(configToken_);
      
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
  analyzeDigis(phase1DigisCEE, detId2IdxCEE_, digis);
  analyzeDigis(phase1DigisCEH, detId2IdxCEH_, digis);
  analyzeDigis(phase1DigisCEHSci, detId2IdxCEHSci_, digis);

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
