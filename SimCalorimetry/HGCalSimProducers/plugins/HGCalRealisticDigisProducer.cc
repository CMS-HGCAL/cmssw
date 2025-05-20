#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/HGCalDigi/interface/HGCROCChannelDataFrame.h"
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalRawDataDefinitions.h"

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"

#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalConfiguration.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"

#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"


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
  
  //ROC-related variables and methods
  bool rocCharMode_; //characterization mode
  HGCROCChannelDataFrame<uint32_t> rocPacker_; //helper class
  std::vector<uint32_t> packInROCframes(hgcaldigi::HGCalDigiHost::ConstView &, size_t , size_t ); //SoA -> ROC format wrapper
    
  //ROC digis to consume
  edm::EDGetTokenT<hgcaldigi::HGCalDigiHost> rocDigisToken_;

  //module mapping
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexToken_;
  
  //FED Raw data
  const edm::EDPutTokenT< std::vector<uint32_t> > fedDataToken_;  
};

//
HGCalRealisticDigisProducer::HGCalRealisticDigisProducer(const edm::ParameterSet& iConfig) :
  rocCharMode_( iConfig.getUntrackedParameter<bool>("ROCCharMode") ),
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
  edm::Handle<hgcaldigi::HGCalDigiHost> rocDigis;
  iEvent.getByToken(rocDigisToken_,rocDigis);
  auto digis_view = rocDigis->const_view();
  
  //loop over FEDs
  auto nerxPerType = moduleIndexer.getGlobalTypesNErx();
  for(const auto &fed : moduleIndexer.getFEDReadoutSequences() ) {

    //loop over readout sequence of this FED
    size_t nmodules = fed.readoutTypes_.size();
    if(nmodules==0) continue;

    
    for(size_t i=0; i<nmodules; i++) {

      //compute the starting and ending indices
      auto readoutType = fed.readoutTypes_[i];
      auto nerx = nerxPerType[readoutType];
      auto idx_i = fed.chDataOffsets_[i];
      auto idx_f = idx_i + 37*nerx;

      //pack DIGIs as ROC words
      std::vector<uint32_t> rocData = packInROCframes(digis_view, idx_i, idx_f);

      //pack in ECON-data
      std::cout << std::dec << i << " " << rocData.size() << " 0x" << std::hex << rocData[0] << std::endl;

    }//end loop over readout sequence
    
  } //end loop over FEDs

  std::vector<uint32_t> fedData;
  

  // put information to the event
  iEvent.emplace(fedDataToken_, std::move(fedData));
}

//
std::vector<uint32_t> HGCalRealisticDigisProducer::packInROCframes(hgcaldigi::HGCalDigiHost::ConstView &digis_view, size_t idx_i, size_t idx_f) {
  
  std::vector<uint32_t> rocData(idx_f-idx_i,0);

  //loop over channel digis and pack in 32b word
  for(size_t i=idx_i; i<idx_f; i++) {
    bool tc = digis_view.tctp()[i] & 0x1;
    bool tp = (digis_view.tctp()[i]>>1) & 0x1;
    rocPacker_.fill(rocCharMode_, tc, tp, digis_view.adcm1()[i], digis_view.adc()[i], digis_view.tot()[i], digis_view.toa()[i]);
    rocData[i-idx_i] = rocPacker_.raw();
  }
  
  return rocData;
}


// fill descriptions
void HGCalRealisticDigisProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.addUntracked<edm::InputTag>("ROCDigis", edm::InputTag("hgcalDigis"));
  desc.addUntracked<bool>("ROCCharMode", false);
  descriptions.addWithDefaultLabel(desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalRealisticDigisProducer);
