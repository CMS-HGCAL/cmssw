#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "SimCalorimetry/HGCalSimAlgos/interface/HGCalRawDataPackingTools.h"


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
  std::vector<uint16_t> buildCommonModeWords(hgcaldigi::HGCalDigiHost::ConstView &, size_t , size_t ); //common mode words

  //ECON-D related variables and methods
  std::vector<uint32_t> packInECONDframes(uint32_t , std::vector<uint32_t> &, std::vector<uint16_t> &, uint32_t bx, uint32_t l1a, uint32_t orb); //ROC frames -> ECON-D
    
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
  //BX, event number and orbit
  uint32_t bx  = iEvent.bunchCrossing();
  uint32_t l1a = iEvent.id().event();
  uint32_t orb = iEvent.orbitNumber(); 
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
      std::vector<uint32_t> rocFrames = packInROCframes(digis_view, idx_i, idx_f);
      std::vector<uint16_t> cm = buildCommonModeWords(digis_view, idx_i, idx_f);

      //pack in ECON-data
      std::vector<uint32_t> econdFrame = packInECONDframes(nerx, rocFrames, cm, bx, l1a, orb);

      std::cout << std::dec << i << " " << rocFrames.size() << " 0x" << std::hex << rocFrames[0] << std::endl;

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

//
std::vector<uint16_t> HGCalRealisticDigisProducer::buildCommonModeWords(hgcaldigi::HGCalDigiHost::ConstView &digis_view, size_t idx_i, size_t nErx) {
  
  std::vector<uint16_t> cmWords(nErx);
  for(size_t i=0; i<nErx; i++) {
    cmWords[i] = digis_view.cm()[idx_i+i*37]/2;
  }
  
  return cmWords;
}



//
std::vector<uint32_t> HGCalRealisticDigisProducer::packInECONDframes(uint32_t nErx, std::vector<uint32_t> &rocFrames,std::vector<uint16_t> &cm, uint32_t bx, uint32_t l1a, uint32_t orb) {

  assert(rocFrames.size()==nErx*37);

  std::vector<uint32_t> econdFrame = hgcal::econd::eventPacketHeader(0x154, nErx*2+rocFrames.size(), true, false, 0, 0, false, false, 0, bx, l1a, orb, false, 0);

  uint64_t chenable(0x1fffffffff); //37 enabled channels
  for(size_t i=0; i<nErx; i++) {

    //start a new erx
    auto cmval = cm[i];
    std::vector<uint32_t> eRxHeader = hgcal::econd::eRxSubPacketHeader(0,0,false,cmval, cmval, chenable);
    econdFrame.insert(econdFrame.end(),eRxHeader.begin(),eRxHeader.end());

    //add data from 37 channels
    auto idx_i = i*37;
    econdFrame.insert(econdFrame.end(),eRxHeader.begin()+idx_i,eRxHeader.begin()+idx_i+37);
  }
  
  econdFrame.push_back(0);
  econdFrame.back() = hgcal::econd::computeCRC(econdFrame);

  return econdFrame;
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
