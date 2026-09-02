#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "SimCalorimetry/HGCalSimProducers/interface/TPG/TPGFEModuleEmulation.hh"
#include "EventFilter/HGCalRawToDigi/interface/TPG/TPGFEDataformat.hh"
#include "EventFilter/HGCalRawToDigi/interface/TPG/TPGBEDataformat.hh"
#include "DataFormats/HGCalDigi/interface/HGCalRawDataDefinitions.h"
#include "DataFormats/HGCalDigi/interface/HGCalTriggerDefinitions.h"

#include "DataFormats/HGCalDigi/interface/HGCalDigiHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiTriggerHost.h"

#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"

#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfiguration.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexerTrigger.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexerTrigger.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"


using namespace hgcal;
//
// class declaration
//

class HGCalTriggerEmulator : public edm::stream::EDProducer<> {
public:
  explicit HGCalTriggerEmulator(const edm::ParameterSet&);
  ~HGCalTriggerEmulator() override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void beginStream(edm::StreamID) override;
  void produce(edm::Event&, const edm::EventSetup&) override;
  void endStream() override;


  // ----------member data ---------------------------
  const edm::EDGetTokenT<hgcaldigi::HGCalDigiHost> digisToken_;
  edm::ESGetToken<HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexInfoToken_;
  edm::ESGetToken<hgcal::HGCalMappingCellParamHost, HGCalElectronicsMappingRcd> cellToken_;
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIdxToken_;
  edm::ESGetToken<HGCalMappingModuleIndexerTrigger, HGCalElectronicsMappingRcd> moduleTriggerIdxToken_;
  edm::ESGetToken<HGCalTriggerConfiguration, HGCalModuleConfigurationRcd> configToken_;

  const edm::EDPutTokenT<hgcaldigi::HGCalDigiTriggerHost> emuldigisTriggerToken_;
};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
HGCalTriggerEmulator::HGCalTriggerEmulator(const edm::ParameterSet& iConfig) 
    : digisToken_(consumes<hgcaldigi::HGCalDigiHost>(iConfig.getParameter<edm::InputTag>("src"))),
      denseIndexInfoToken_(esConsumes()),
      cellToken_(esConsumes()),
      moduleIdxToken_(esConsumes()),
      moduleTriggerIdxToken_(esConsumes()),
      configToken_(esConsumes()),
      emuldigisTriggerToken_(produces<hgcaldigi::HGCalDigiTriggerHost>()){}

HGCalTriggerEmulator::~HGCalTriggerEmulator() {}
//
// member functions
//

// ------------ method called to produce the data  ------------
void HGCalTriggerEmulator::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  using namespace edm;

  const auto& config = iSetup.getData(configToken_);

  const auto& digis = iEvent.getHandle(digisToken_);
  const auto& digis_view = digis->const_view();
  int32_t ndigis = digis_view.metadata().size();

  auto const& cellInfo = iSetup.getData(cellToken_);
  auto const& cellInfo_view = cellInfo.const_view();
  const auto& denseIndexInfo = iSetup.getData(denseIndexInfoToken_);
  const auto& denseIndexInfo_view = denseIndexInfo.const_view();
  int32_t ndii = denseIndexInfo_view.metadata().size();
  assert( ndigis == ndii );

  const HGCalMappingModuleIndexer& moduleIndexer = iSetup.getData(moduleIdxToken_);
  const HGCalMappingModuleIndexerTrigger& moduleTriggerIndexer = iSetup.getData(moduleTriggerIdxToken_);
  hgcaldigi::HGCalDigiTriggerHost emuldigisTrigger(cms::alpakatools::host(), moduleIndexer.maxDataSize());

  for (int32_t i = 0; i < emuldigisTrigger.view().metadata().size(); i++) {
    for (int32_t ibx = 0; ibx < 7; ibx++) emuldigisTrigger.view()[i].valid()(ibx,0) = false;
    emuldigisTrigger.view()[i].algo() = 0;
  }

  TPGFEConfiguration::Configuration cfgs;
  cfgs.initId();

  std::map<std::string, std::pair<uint32_t, uint32_t>> typecodeMap = moduleTriggerIndexer.typecodeMap();
  uint16_t bx = 0; // FIXME implement extended reading

  for(const auto& frs :  moduleTriggerIndexer.fedReadoutSequences() ) {
    if (frs.readoutTypes_.empty()) {
      continue;
    }
    auto fedId = frs.id;
    uint32_t globalEcontIdx = 0;  


    LogDebug("[HGCalTriggerEmulator]") << "Emulator:: starts emulation of Fed Id: " << fedId << std::endl;
    HGCalTriggerFedConfig fedConfig = config.feds[fedId];

    for (std::size_t itdaq = 0; itdaq < fedConfig.tdaqs.size(); itdaq++) {
      HGCalTDAQConfig tdaqConfig = fedConfig.tdaqs[itdaq];
      LogDebug("[HGCalTriggerEmulator]") << "fed[" << std::dec << fedId << "].tdaq[" << itdaq 
            << "], headerMarker = 0x" << std::hex << std::setfill('0') << std::setw(8) << tdaqConfig.tdaqBlockHeaderMarker << std::endl;
      if (tdaqConfig.econts.size()==0) {
        LogDebug("[HGCalTriggerEmulator]") << "with no active ECON-Ts, skipped" << std::endl;
        continue;
      }
    
      LogDebug("[HGCalTriggerEmulator]") << " with " << std::dec << tdaqConfig.econts.size() << " active ECON-Ts" << std::endl;
      for(unsigned int iecont=0; iecont<tdaqConfig.econts.size(); iecont++){
        std::map<uint32_t, TPGFEDataformat::HalfHgcrocData> rocData;

        // getting typecode of econt by inverting the typecode map 
        std::string typecode;

        for (const auto& it : typecodeMap){
          if (it.second == std::make_pair(fedId, globalEcontIdx)) typecode = it.first;
        
        }
        if (typecode.substr(0,1) == "T"){
          LogDebug("[HGCalTriggerEmulator]") << "Typecode : " << typecode << " Tiles NOT yet implemented, skipping!" << std::endl;
          globalEcontIdx++; // counter for total econts
          continue;
        }

        std::string short_typecode = typecode.substr(0,4); 
              
        LogDebug("[HGCalTriggerEmulator]") << "----------------- iecont " << globalEcontIdx 
                  << " typecode " << typecode 
                  << "-------------------"<<std::endl;
        LogDebug("[HGCalTriggerEmulator]") << "\n--- Preapera hgroc cfg and read ADC from DAQ DIGIs ---" << std::endl;

        // getting the first idx of DAQ ADC data
        uint32_t digi_idx =  moduleIndexer.getIndexForModuleData(typecode);

        uint32_t nhfrocs = moduleIndexer.getNumERxs(typecode);
        LogDebug("[HGCalTriggerEmulator]") << "Number of half rocs " << nhfrocs << std::endl;

        HGCalECONTConfig econtConfig = tdaqConfig.econts[iecont];
        cfgs.setEconTConfig(globalEcontIdx, econtConfig);
        
        std::map< uint32_t, std::vector<uint32_t> >  SiTCToROCpin;
   
        for (uint32_t ihroc = 0; ihroc < nhfrocs; ++ihroc) {

          cfgs.setRocConfig(ihroc, econtConfig);
          TPGFEDataformat::HalfHgcrocData hData;
          hData.setBx(bx);

          LogDebug("[HGCalTriggerEmulator]") << "ihroc : " << ihroc << " | Setting ADC for half HGROC channels." << std::endl;


          for (uint32_t ch = 0; ch < 37; ++ch)  {

            uint32_t cellInfoIdx(denseIndexInfo_view.cellInfoIdx()[digi_idx]);

            //auto indexinfo = denseIndexInfo_view[digi_idx];
            auto digidaq = digis_view[digi_idx];
            uint16_t rocpin = cellInfo_view.rocpin()[cellInfoIdx];
            uint16_t roc = cellInfo_view.chip()[cellInfoIdx];
            uint16_t half = cellInfo_view.half()[cellInfoIdx];
            uint16_t TrLink = cellInfo_view.triglink()[cellInfoIdx];
            uint16_t TrCell = cellInfo_view.trigcell()[cellInfoIdx];
            //uint32_t chidx = indexinfo.chNumber(); //


            if (TrLink == uint16_t(-1) || TrCell == uint16_t(-1)) { 
              LogDebug("[HGCalTriggerEmulator]") << " \t Emulator::SettingADC:: ch " << ch << " rocpin " << rocpin << " is calibration (0), unconnected (-1), or not used in trigger sum (1): " << cellInfo_view.t()[cellInfoIdx]  << ", skipping!" << std::endl;
              digi_idx++;
              continue;

            } 
            uint32_t absTC = (cellInfo_view.isHD()[cellInfoIdx]==0) ? (roc*16 + TrLink*4 + TrCell) : (roc*8 + TrLink*2 + TrCell) ;
            uint32_t adc = digidaq.adc();

            LogDebug("[HGCalTriggerEmulator]") << "\t rocpin " << rocpin << " chip " << roc << " TrLink " << TrLink << " TrCell " << TrCell << " TC " << absTC << std::endl;          
            LogDebug("[HGCalTriggerEmulator]") << "\t ihROC " << ihroc << " ROC" << roc << " Half " << half << " rocpin " << rocpin << " ch " << roc*72 + rocpin << " adc "<< adc <<std::endl;

            hData.getChannelData(rocpin%36).setAdc(adc, 0);    
            SiTCToROCpin [absTC].push_back(roc*72 + rocpin);
            digi_idx++;

          }
          rocData[ihroc] = hData;

        }
        LogDebug("[HGCalTriggerEmulator]") << "Size of the TC to ROCpin map: " << SiTCToROCpin.size() << std::endl;
        cfgs.setSiTCToChModule(SiTCToROCpin);


        LogDebug("[HGCalTriggerEmulator]") << "\n--- Running HGCROC Emulation ---" << std::endl;
        TPGFEModuleEmulation::HGCROCTPGEmulation rocEmul(cfgs);
        TPGFEDataformat::ModuleTcData modTcData;
        rocEmul.Emulate(false, short_typecode, globalEcontIdx, rocData, modTcData);

    
        LogDebug("[HGCalTriggerEmulator]") << "\n--- Running ECON-T Emulation ---" << std::endl;
        TPGFEModuleEmulation::ECONTEmulation econtEmul(cfgs);
        econtEmul.disableTcSafety = true;
        TPGFEDataformat::TcRawDataPacket rdp;
        econtEmul.Emulate(false, short_typecode, globalEcontIdx, modTcData, rdp);
        //rdp.print();
        
        
        uint32_t totE = 0; // module sum, for BC is over all the 48 TCs 
        for(const auto& itc: rdp.getTcData()) totE += itc.decodedE(rdp.type()) >> cfgs.getEconTPara().at(globalEcontIdx).getDropLSB();
        
        for(unsigned itc(0) ; itc < rdp.size() ; itc++){

          uint32_t tcidx = uint32_t(rdp.getTc(itc).address()); 

          uint32_t denseIdx = moduleTriggerIndexer.getIndexForModuleData(fedId, globalEcontIdx, tcidx) ; // before any swapping
          
          emuldigisTrigger.view()[denseIdx].algo() = uint8_t(cfgs.getEconTPara().at(globalEcontIdx).getOutType());
          emuldigisTrigger.view()[denseIdx].sumType() = uint8_t(cfgs.getEconTPara().at(globalEcontIdx).getMSSumType());
          emuldigisTrigger.view()[denseIdx].valid()(bx,0) = true;
          emuldigisTrigger.view()[denseIdx].nBxs() = uint8_t(1);
          emuldigisTrigger.view()[denseIdx].econTId() = globalEcontIdx;
          emuldigisTrigger.view()[denseIdx].nTCs() = uint8_t(cfgs.getEconTPara().at(globalEcontIdx).getNofTCs());
          emuldigisTrigger.view()[denseIdx].econtHeader()(bx,0) = uint8_t(rdp.bx());
          emuldigisTrigger.view()[denseIdx].expEcontHeader()(bx,0) = uint8_t(0);
          emuldigisTrigger.view()[denseIdx].encodedTotE()(bx,0) = (rdp.type()==TPGFEDataformat::BestC)? uint32_t(rdp.moduleSum()) : totE ;
          emuldigisTrigger.view()[denseIdx].TotE()(bx,0) = (rdp.type()==TPGFEDataformat::BestC)? uint32_t(TPGFEDataformat::TcRawData::Decode5E3M(rdp.moduleSum())) : totE ;
          emuldigisTrigger.view()[denseIdx].TCEnergy()(bx,0) = uint32_t(rdp.getTc(itc).decodedE(rdp.type()) << cfgs.getEconTPara().at(globalEcontIdx).getDropLSB());
          emuldigisTrigger.view()[denseIdx].encodedTCEnergy()(bx,0) = uint32_t(rdp.getTc(itc).energy());
          emuldigisTrigger.view()[denseIdx].TCAddress()(bx,0) = uint8_t(rdp.getTc(itc).address());

          LogDebug("[HGCalTriggerEmulator]")  << " fedId : " << fedId
                  << ", globalEcontIdx: " << globalEcontIdx
                  << ", tcidx: " << tcidx
                  << ", denseIdx: " << denseIdx
                  << std::endl;
          LogDebug("[HGCalTriggerEmulator]")  << " algo = " << uint16_t(emuldigisTrigger.view()[denseIdx].algo())
                  << " sumType = " << uint16_t(emuldigisTrigger.view()[denseIdx].sumType())
                  << ", valid = " << uint16_t(emuldigisTrigger.view()[denseIdx].valid()(bx,0))
                  << ", nBxs = " << uint16_t(emuldigisTrigger.view()[denseIdx].nBxs())
                  << ", nTCs = " << uint16_t(emuldigisTrigger.view()[denseIdx].nTCs())
                  << ", ieconTId = " << uint32_t(emuldigisTrigger.view()[denseIdx].econTId())
                  << std::endl;
          LogDebug("[HGCalTriggerEmulator]")  << " ibx : " << bx
                  << ", econt header : " << uint16_t(emuldigisTrigger.view()[denseIdx].econtHeader()(bx,0))
                  << ", expected econt header from Slink : " << uint16_t(emuldigisTrigger.view()[denseIdx].expEcontHeader()(bx,0))
                  << ", MS/totE : " << uint32_t(emuldigisTrigger.view()[denseIdx].TotE()(bx,0))
                  << std::endl;
          LogDebug("[HGCalTriggerEmulator]")  << " itc : " << itc
                  << ", Address: " << uint16_t(emuldigisTrigger.view()[denseIdx].TCAddress()(bx,0))
                  << ", Encoded Energy: " << uint32_t(emuldigisTrigger.view()[denseIdx].encodedTCEnergy()(bx,0))
                  << ", Unpacked Energy: " << uint32_t(emuldigisTrigger.view()[denseIdx].TCEnergy()(bx,0))
                  << ", Encoded MS: " << uint32_t(emuldigisTrigger.view()[denseIdx].encodedTotE()(bx,0))
                  << ", Unpacked MS: " << uint32_t(emuldigisTrigger.view()[denseIdx].TotE()(bx,0))
                  << std::endl;
        }
        globalEcontIdx++; // counter for total econts in fed
      } 
    }
  }
  iEvent.emplace(emuldigisTriggerToken_, std::move(emuldigisTrigger));

}

// ------------ method called once each stream before processing any runs, lumis or events  ------------
void HGCalTriggerEmulator::beginStream(edm::StreamID) {
  // please remove this method if not needed
}

// ------------ method called once each stream after processing all runs, lumis and events  ------------
void HGCalTriggerEmulator::endStream() {
  // please remove this method if not needed
}


// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void HGCalTriggerEmulator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("src", edm::InputTag("hgcalDigi"));
  descriptions.add("hgcalEmulDigisTrigger", desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(HGCalTriggerEmulator);
