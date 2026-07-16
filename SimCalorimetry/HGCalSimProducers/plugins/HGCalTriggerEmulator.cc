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
#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfiguration.h"
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
      configToken_(esConsumes()) {}
      //digisEmulatorTrigToken_(produces<hgcaldigi::HGCalDigiTrigEmulatorHost>()) {} // FIXME create a class to save the emulated digis
      

HGCalTriggerEmulator::~HGCalTriggerEmulator() {}
//
// member functions
//

// ------------ method called to produce the data  ------------
void HGCalTriggerEmulator::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  const auto& config = iSetup.getData(configToken_);

  uint32_t ifed = 1600; // FIXME read fed id from cfg

  std::cout << "--------------------  Ciao, I am dummy emulator ! -------------------------------- " << std::endl;

  HGCalTriggerFedConfig fedConfig = config.feds[ifed];

  TPGFEConfiguration::Configuration cfgs;
  // Load channel mappings
  cfgs.setSiChMapFile("/data/fcetorel/work/tpgEmulator/CMSSW_16_1_0/src/HGCalCommissioning/Calibrations/P5/maps/WaferCellMapTraces.txt");
  //cfgs.setSciChMapFile("cfgmap/channels_sipmontile_HDtypes.hgcal.txt");
  cfgs.initId();
  cfgs.readSiChMapping();
  //cfgs.readSciChMapping();
  cfgs.loadMuxMapping();


  
  // FIXME read adc from max digis, here hardcoded for TB 2026 scenario
  uint32_t globalEcontIdx = 0;
  uint32_t globalhROCIdx = 0;
  std::vector<uint32_t> maxinputDaq = {120, 126, 132, 138, 144, 150, 156, 168, 162, 333, 777, 174, 180}; 
  std::map<uint32_t, TPGFEDataformat::HalfHgcrocData> rocData;
  uint16_t bx = 0;

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
      std::cout << "-------------------- iecont " << globalEcontIdx << "--------------------------------"<<std::endl;
      std::string typecode = "ML-F";  // FIXME read type code from CFG
      uint32_t nhfrocs = cfgs.getSiModNhroc(typecode);
      
      HGCalECONTConfig econtConfig = tdaqConfig.econts[iecont];

      cfgs.setEconTConfig(globalEcontIdx, econtConfig);
      uint32_t fixedADC = maxinputDaq[globalEcontIdx];

      std::cout << "\n--- Preapera hgroc cfg and Initializing Mock ADC Data ---" << std::endl;
      const std::map<std::pair<std::string,uint32_t>,uint32_t> RocPinToAbs = cfgs.getSiRocpinToAbsSeq();
      for (uint32_t ihroc = 0; ihroc < nhfrocs; ++ihroc) {

    

        cfgs.setRocConfig(globalhROCIdx, econtConfig);
        TPGFEDataformat::HalfHgcrocData hData;
        hData.setBx(bx);

        std::cout << "ROC ID: " << globalhROCIdx << " | Setting ADC to " << fixedADC << " for channels." << std::endl;
        uint32_t half = (ihroc%2 == 0 ? 1 : 0 );
        for (uint32_t ch = 0; ch < 36; ++ch)  {
          
          if (RocPinToAbs.find(std::make_pair(typecode, ch )) == RocPinToAbs.end())
          {
            std::cout << "Emulator::SettingMockADC:: ROC " << ihroc/2 << " Half " << half << " << Ch: " << ch << " is unconnected: " << "Skipping!" << std::endl;
            continue;
          }
          //std::cout << "ROC " << ihroc/2 << " Half " << half << " Ch " << ch << " adc "<< fixedADC <<std::endl;

          hData.getChannelData(ch).setAdc(fixedADC, 0);    



        }
        rocData[ihroc] = hData;

        fixedADC++; // one adc more for every hfroc
        globalhROCIdx++;
      }

   
      std::cout << "\n--- Running HGCROC Emulation ---" << std::endl;
      TPGFEModuleEmulation::HGCROCTPGEmulation rocEmul(cfgs);
      std::map<uint32_t, TPGFEDataformat::ModuleTcData> allModTcData;
      rocEmul.Emulate(false, globalEcontIdx, rocData, allModTcData);

      
      std::cout << "\n--- Running ECON-T Emulation ---" << std::endl;
      TPGFEModuleEmulation::ECONTEmulation econtEmul(cfgs);
      econtEmul.disableTcSafety = true;
      TPGFEDataformat::TcModulePacket econtOutput;

      econtEmul.Emulate(false, globalEcontIdx, allModTcData, econtOutput);
      econtOutput.second.print();

      globalEcontIdx++; // counter for total econts
    }
  }
std::cout << "--------------------  Dummy emulator end -------------------------------- " << std::endl;


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
