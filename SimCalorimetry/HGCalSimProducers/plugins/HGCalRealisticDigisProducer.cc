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

class HGCalRealisticDigisProducer : public edm::stream::EDProducer<> {
  /**
     @this class consumes the ROC SoA DIGIs and creates the FEDRawData of HGCAL
   */
public:
  explicit HGCalRealisticDigisProducer(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  
private:
  void beginStream(edm::StreamID) override {}
  void produce(edm::Event&, const edm::EventSetup&) override;
  void endStream() override {}
  void beginRun(edm::Run const&, edm::EventSetup const&) override;

  //ROC digis to consume
  edm::EDGetTokenT<hgcaldigi::HGCalDigiHost> rocDigisToken_;
  
  //module mapping
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexToken_;
  
  //FED Raw data
  const edm::EDPutTokenT< std::vector<uint32_t> > fedDataToken_;  
};

//
HGCalRealisticDigisProducer::HGCalRealisticDigisProducer(const edm::ParameterSet& iConfig) :
  rocDigisToken_( consumes<hgcaldigi::HGCalDigiHost>( iConfig.getUntrackedParameter<edm::InputTag>("ROCDigis") ) ),
  moduleIndexToken_(esConsumes()),
  fedDataToken_(produces< std::vector<uint32_t> >())
{
}

//
void HGCalRealisticDigisProducer::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) {
}

//
void HGCalRealisticDigisProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  std::cout << " [HGCalRealisticDigisProducer] produce" << std::endl;
  
  // retrieve logical mapping and dense indexing
  const auto& moduleIndexer = iSetup.getData(moduleIndexToken_);

  // retrieve the ROC Digis
  edm::Handle<hgcaldigi::HGCalDigiHost> rocDigis_;
  iEvent.getByToken(rocDigisToken_,rocDigis_);

  std::cout << rocDigis_.isValid() << std::endl;

  //loop over FEDs
  auto nerxPerType = moduleIndexer.getGlobalTypesNErx();
  for(const auto &fed : moduleIndexer.getFEDReadoutSequences() ) {

    //loop over readout sequence of this FED
    size_t nmodules = fed.readoutTypes_.size();
    if(nmodules==0) continue;

    std::cout << "FED id: " << fed.id << " has " << nmodules << " modules" << std::endl;
    for(size_t i=0; i<nmodules; i++) {
      
      auto readoutType = fed.readoutTypes_[i];
      auto nerx = nerxPerType[readoutType];
      std::cout << "\t " << i << " : " << fed.chDataOffsets_[i] << " -> i+" << 37*nerx << std::endl;
    }//end loop over readout sequence
    
  } //end loop over FEDs

  std::vector<uint32_t> fedData;
  

  // put information to the event
  iEvent.emplace(fedDataToken_, std::move(fedData));
}

// fill descriptions
void HGCalRealisticDigisProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.addUntracked<edm::InputTag>("ROCDigis", edm::InputTag("hgcalDigis"));
  descriptions.addWithDefaultLabel(desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalRealisticDigisProducer);
