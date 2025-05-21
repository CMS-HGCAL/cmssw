#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "SimCalorimetry/HGCalSimAlgos/interface/HGCalRawDataPackingTools.h"

#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"
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
  std::vector<uint32_t> packInECONDframes(uint32_t , std::vector<uint32_t> &, std::vector<uint16_t> &, uint32_t , uint32_t , uint32_t ); //ROC frames -> ECON-D
  
  //Capture Block (miniDAQ) related methods
  std::vector<uint32_t> buildMiniDAQframe(std::vector<uint32_t>& , uint32_t , uint32_t , uint32_t , uint32_t ); //MiniDAQ frames

  //FED-level related method
  std::vector<uint32_t> buildFEDframe(hgcaldigi::HGCalDigiHost::ConstView &, const HGCalMappingModuleIndexer &, uint32_t , uint32_t , uint32_t , uint32_t ); //FED frame
    
  //ROC digis to consume
  edm::EDGetTokenT<hgcaldigi::HGCalDigiHost> rocDigisToken_;

  //module mapping
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexToken_;
  
  //FED Raw data
  const edm::EDPutTokenT<FEDRawDataCollection> fedDataToken_;
};

//
HGCalRealisticDigisProducer::HGCalRealisticDigisProducer(const edm::ParameterSet& iConfig) :
  rocCharMode_( iConfig.getUntrackedParameter<bool>("ROCCharMode") ),
  rocDigisToken_( consumes<hgcaldigi::HGCalDigiHost>( iConfig.getUntrackedParameter<edm::InputTag>("ROCDigis") ) ),
  moduleIndexToken_(esConsumes()),
  fedDataToken_(produces<FEDRawDataCollection>()) {
}

//
void HGCalRealisticDigisProducer::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) {
}

//
void HGCalRealisticDigisProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  FEDRawDataCollection buffers;
  
  //BX, event number and orbit
  uint32_t bx  = iEvent.bunchCrossing();
  uint32_t l1a = iEvent.id().event();
  uint32_t orb = iEvent.orbitNumber(); 
  std::cout << " [HGCalRealisticDigisProducer] produce L1A=" << l1a << " BX=" << bx << " Orbit=" << orb << std::endl;
  
  // retrieve logical mapping and dense indexing
  const auto& moduleIndexer = iSetup.getData(moduleIndexToken_);

  // retrieve the ROC Digis
  edm::Handle<hgcaldigi::HGCalDigiHost> rocDigis;
  iEvent.getByToken(rocDigisToken_, rocDigis);
  auto digis_view = rocDigis->const_view();
  
  //loop over FEDs
  for (uint32_t ifed=0; ifed<moduleIndexer.fedCount(); ++ifed) {
    
    const size_t nmodules = moduleIndexer.getFEDReadoutSequences()[ifed].readoutTypes_.size();
    if (nmodules == 0) continue;

    //build the dataframe
    auto fedid = moduleIndexer.getFEDReadoutSequences()[ifed].id;
    std::vector<uint32_t> fed_frame = buildFEDframe(digis_view, moduleIndexer,ifed,bx,l1a,orb);
    auto fed_frame_size = fed_frame.size()*sizeof(uint32_t)/sizeof(char);
    
    //store in FED data
    auto& fed_data = buffers.FEDData(fedid);
    fed_data.resize(fed_frame_size);
    auto* ptr = fed_data.data();
    std::memcpy(ptr, fed_data.data(), fed_frame_size);
  
  } // end FED loop

  //put data in event
  iEvent.emplace(fedDataToken_, std::move(buffers));
}

//
std::vector<uint32_t> HGCalRealisticDigisProducer::buildFEDframe(hgcaldigi::HGCalDigiHost::ConstView &digis_view,const HGCalMappingModuleIndexer &moduleIndexer, uint32_t ifed, uint32_t bx, uint32_t l1a, uint32_t orb) {

  //raw data at different levels that needs to be packed to fedData
  std::vector<uint32_t> fedData, cbPayload, econdFrame, rocFrames;
  std::vector<uint16_t> cm;

  //factorize the operation of adding another miniDAQ to this FED data
  uint32_t cur_cb = 0xffffffff;
  uint32_t cb_idx = cur_cb;
  uint32_t necons = 0;
  auto _flushCaptureBlock = [&]() {
    if(cur_cb==cb_idx) return cur_cb;
    cur_cb = cb_idx;
    if(necons==0) return cur_cb;
    
    const auto miniDAQData = buildMiniDAQframe(cbPayload, necons, bx, l1a, orb);
    fedData.insert(fedData.end(), miniDAQData.begin(), miniDAQData.end());

    necons = 0;
    cbPayload.clear();

    return cur_cb;
  };

  //loop over readout sequence in FED
  auto fed = moduleIndexer.getFEDReadoutSequences()[ifed];
  auto nerxPerType = moduleIndexer.getGlobalTypesNErx();
  uint32_t nmodules = fed.readoutTypes_.size();
  for (uint32_t i = 0; i < nmodules; ++i) {

    //check capture block and try to flush
    const uint32_t dense_idx = fed.moduleLUT_[i];
    assert(i == dense_idx);
    cb_idx = moduleIndexer.getFEDIndexer().unpackDenseIndex(dense_idx)[0];
    _flushCaptureBlock();
    
    //determine dense index range to read channel data from
    const auto readoutType = fed.readoutTypes_[i];
    const auto nerx = nerxPerType[readoutType];
    const auto idx_i = fed.chDataOffsets_[i];
    const auto idx_f = idx_i + 37 * nerx;
    rocFrames = packInROCframes(digis_view, idx_i, idx_f);
    assert(rocFrames.size() == 37 * nerx);
    cm = buildCommonModeWords(digis_view, idx_i, nerx);
    assert(cm.size() == nerx);

    //pack the ROC frames in  the ECON-D
    econdFrame = packInECONDframes(nerx, rocFrames, cm, bx, l1a, orb);
    assert(econdFrame.size() == 2 + 39 * nerx + 1);

    //add to the miniDAQ
    cbPayload.insert(cbPayload.end(), econdFrame.begin(), econdFrame.end());
    ++necons;
  }

  _flushCaptureBlock();  // Final block per FED

  //finalise the fed data
  uint32_t slink_content = hgcal::backend::buildSlinkContentId(hgcal::backend::SlinkEmulationFlag::Subsystem,0,0);
  std::vector<uint32_t> fed_header = hgcal::backend::buildSlinkHeader(0, 0, l1a, slink_content, fed.id);
  fedData.insert(fedData.end(), fed_header.begin(), fed_header.end());
  uint16_t slink_status = hgcal::backend::buildSlinkRocketStatus(false, false, false, false, false);
  std::vector<uint32_t> fed_trailer = hgcal::backend::buildSlinkTrailer(0, 0, fedData.size()/8, bx, orb, 0, slink_status);
  fedData.insert(fedData.end(), fed_trailer.begin(), fed_trailer.end());

  return fedData;
}

//
std::vector<uint32_t> HGCalRealisticDigisProducer::buildMiniDAQframe(std::vector<uint32_t>& econdpayload, uint32_t econs, uint32_t bx, uint32_t l1a, uint32_t orb) {

    std::vector<uint8_t> econd_statuses(econs, 0);
    std::vector<uint32_t> cbheader = hgcal::backend::buildCaptureBlockHeader(bx, l1a, orb, econd_statuses);

    size_t total_size = cbheader.size() + econdpayload.size();
    size_t remainder = total_size % 4;
    if (remainder != 0) {
        size_t padding_needed = 4 - remainder;
        econdpayload.insert(econdpayload.end(), padding_needed, 0);
    }

    std::vector<uint32_t> output;
    output.reserve(cbheader.size() + econdpayload.size());
    output.insert(output.end(), cbheader.begin(), cbheader.end());
    output.insert(output.end(), econdpayload.begin(), econdpayload.end());

    return output;
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


// fill descriptions
void HGCalRealisticDigisProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.addUntracked<edm::InputTag>("ROCDigis", edm::InputTag("hgcalDigis"));
  desc.addUntracked<bool>("ROCCharMode", false);
  descriptions.addWithDefaultLabel(desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalRealisticDigisProducer);
