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
  const edm::EDPutTokenT<hgcaldigi::HGCalDigiTriggerHost> digisTriggerToken_;
  edm::ESGetToken<HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexInfoToken_;
  edm::ESGetToken<hgcal::HGCalMappingCellParamHost, HGCalElectronicsMappingRcd> cellToken_;
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIdxToken_;
  edm::ESGetToken<HGCalMappingModuleIndexerTrigger, HGCalElectronicsMappingRcd> moduleTriggerIdxToken_;
  edm::ESGetToken<HGCalTriggerConfiguration, HGCalModuleConfigurationRcd> configToken_;
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
      configToken_(esConsumes()) {}
     // FIXME save the output of emulator in a collection 

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

  TPGFEConfiguration::Configuration cfgs;
  // Load channel mappings
  cfgs.setSiChMapFile("/data/fcetorel/work/tpgEmulator/CMSSW_16_1_0/src/HGCalCommissioning/Calibrations/P5/maps/WaferCellMapTraces.txt");
  //cfgs.setSciChMapFile("cfgmap/channels_sipmontile_HDtypes.hgcal.txt");
  cfgs.initId();
  cfgs.readSiChMapping();
  //cfgs.readSciChMapping();
  cfgs.loadMuxMapping();

   
  std::map<std::string, std::pair<uint32_t, uint32_t>> typecodeMap = moduleTriggerIndexer.typecodeMap();
  uint32_t globalEcontIdx = 0;  
  uint16_t bx = 0; // FIXME implement extended reading

  for(const auto& frs :  moduleTriggerIndexer.fedReadoutSequences() ) {
    if (frs.readoutTypes_.empty()) {
      continue;
    }
    auto ifed = frs.id;

    std::cout << "Emulator:: starts emulation of Fed Id: " << ifed << std::endl;
    HGCalTriggerFedConfig fedConfig = config.feds[ifed];

    const std::map<std::pair<std::string,uint32_t>,uint32_t> RocPinToAbs = cfgs.getSiRocpinToAbsSeq();

    //const std::map<std::pair<std::string,uint32_t>,uint32_t> SiTCToROCpin = cfgs.getSiTCToROCpin();
    // for (const auto& [key, value] : SiTCToROCpin) {
    //           std::cout << key.first << " TC" <<  key.second << " : rocpin " << value << '\n';
    // }  
    for (std::size_t itdaq = 0; itdaq < fedConfig.tdaqs.size(); itdaq++) {
      HGCalTDAQConfig tdaqConfig = fedConfig.tdaqs[itdaq];
      //std::cout << "fed[" << std::dec << ifed << "].tdaq[" << itdaq 
      //      << "], headerMarker = 0x" << std::hex << std::setfill('0') << std::setw(8) << tdaqConfig.tdaqBlockHeaderMarker << std::endl;
      if (tdaqConfig.econts.size()==0) {
        //std::cout << "with no active ECON-Ts, skipped" << std::endl;
        continue;
      }
    
      //std::cout << " with " << std::dec << tdaqConfig.econts.size() << " active ECON-Ts" << std::endl;
      for(unsigned int iecont=0; iecont<tdaqConfig.econts.size(); iecont++){
        std::map<uint32_t, TPGFEDataformat::HalfHgcrocData> rocData;

        // getting typecode of econt by inverting the typecode map 
        std::string typecode;

        for (const auto& it : typecodeMap){
          if (it.second == std::make_pair(ifed, globalEcontIdx)) typecode = it.first;
        
        }
        if (typecode.substr(0,1) == "T"){
          std::cout << "Typecode : " << typecode << " Tiles NOT yet implemented, skipping!" << std::endl;
          globalEcontIdx++; // counter for total econts
          continue;
        }

        std::string short_typecode = typecode.substr(0,4); 
  
        //std::cout << "typecode " << typecode << " and short " << short_typecode <<  std::endl;
        
      
        std::cout << "----------------- iecont " << globalEcontIdx 
                  << " typecode " << typecode 
                  << " module type " << moduleTriggerIndexer.getTypeForModule(ifed,globalEcontIdx) 
                  << "-------------------"<<std::endl;
        std::cout << "\n--- Preapera hgroc cfg and read ADC from DAQ DIGIs ---" << std::endl;


        // getting the first idx of DAQ ADC data
        uint32_t digi_idx =  moduleIndexer.getIndexForModuleData(typecode);
  
        uint32_t nhfrocs = cfgs.getSiModNhroc(short_typecode);


        std::cout << "Number of half rocs " << nhfrocs << std::endl;

        HGCalECONTConfig econtConfig = tdaqConfig.econts[iecont];
        cfgs.setEconTConfig(globalEcontIdx, econtConfig);

   
        for (uint32_t ihroc = 0; ihroc < nhfrocs; ++ihroc) {

          cfgs.setRocConfig(ihroc, econtConfig);
          TPGFEDataformat::HalfHgcrocData hData;
          hData.setBx(bx);

          std::cout << "ihroc : " << ihroc << " | Setting ADC for HGROC channels." << std::endl;


          for (uint32_t ch = 0; ch < 37; ++ch)  {

            uint32_t cellInfoIdx(denseIndexInfo_view.cellInfoIdx()[digi_idx]);
            if (cellInfo_view.iscalib()[cellInfoIdx]) {
              std::cout << " \t Emulator::SettingADC:: ch " << ch << " is calibration, " << " skipping!" << std::endl;
              digi_idx++;
              continue;

            } 

            auto indexinfo = denseIndexInfo_view[digi_idx];
            auto digidaq = digis_view[digi_idx];

            uint32_t chidx = indexinfo.chNumber();
            uint32_t adc = digidaq.adc();
            uint32_t nhroc = chidx/37;
            uint32_t hrocch = chidx%37;

            uint32_t nroc = chidx/72;


            // if (RocPinToAbs.find(std::make_pair(short_typecode, rocpin )) == RocPinToAbs.end())
            // {
            //   std::cout << " \t Emulator::SettingADC:: half ROC " << nhroc  << " ch: " << ch << " is unconnected: " << "Skipping!" << std::endl;
            //   digi_idx++;

            //   continue;
            // }
            std::cout << "\t half ROC " << nhroc << " Ch " << hrocch << " adc "<< adc <<std::endl;
            if (hrocch > 37) std::cout << " \t Warning! : half roc ch is " << hrocch << " > 37 " << std::endl;
            hData.getChannelData(hrocch).setAdc(adc, 0);    
            
            digi_idx++;

          }
          rocData[ihroc] = hData;

        }


        std::cout << "\n--- Running HGCROC Emulation ---" << std::endl;
        TPGFEModuleEmulation::HGCROCTPGEmulation rocEmul(cfgs);
        std::map<uint32_t, TPGFEDataformat::ModuleTcData> allModTcData;
        rocEmul.Emulate(false, short_typecode, globalEcontIdx, rocData, allModTcData);

    
        std::cout << "\n--- Running ECON-T Emulation ---" << std::endl;
        TPGFEModuleEmulation::ECONTEmulation econtEmul(cfgs);
        econtEmul.disableTcSafety = true;
        TPGFEDataformat::TcModulePacket econtOutput;

        econtEmul.Emulate(false, short_typecode, globalEcontIdx, allModTcData, econtOutput);
        econtOutput.second.print();

        globalEcontIdx++; // counter for total econts
      }
    }
  }


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
  desc.setUnknown();
  descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(HGCalTriggerEmulator);
