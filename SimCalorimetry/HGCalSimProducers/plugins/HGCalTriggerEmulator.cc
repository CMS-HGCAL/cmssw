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
  
  // FIXME read adc from max digis, here hardcoded for TB 2026 scenario
  uint32_t iec = 0;
  std::vector<uint32_t> maxinputDaq = {125, 131, 137, 143, 149, 155, 161, 173, 167, 333, 777, 179, 185}; 

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
      std::cout << "-------------------- iecont " << iec << "--------------------------------"<<std::endl;

      
      HGCalECONTConfig econtConfig = tdaqConfig.econts[iecont];
      TPGFEConfiguration::ConfigEconT cfgecont;
      cfgecont.setSelect(econtConfig.select); // need this to get the type algo later

      uint32_t calib =  0x800; // FIXME correct values and read from cfg (they should be in hexa 0x800, corresponding to 1)

      //dummy daq input
      uint32_t fixed_adc = maxinputDaq[iec];

      // dummy emulate roc
      uint16_t hgrocComp = TPGFEModuleEmulation::CompressHgroc(fixed_adc*4, econtConfig.density);

      // dummy emulate ECONT
      uint64_t decompressed = TPGFEModuleEmulation::DecompressEcont(hgrocComp, !(econtConfig.density)); 
      uint64_t decomp64bit = decompressed * calib; //overflows for 12bit TOT
      decompressed =  decomp64bit >> 11;

      TPGFEDataformat::Type algo = cfgecont.getOutType();
      uint16_t compressed = 0;
      if (algo == TPGFEDataformat::BestC) compressed = TPGFEModuleEmulation::CompressEcontBc(decompressed, econtConfig.dropLSB);  // FIXME implement other algos
      else{
        std::cout << "algo not recognized, skipping econt ..." << std::endl;
        continue;
      }
      // dummy "unpacking"
      uint64_t tc_energy = TPGFEModuleEmulation::decodeEmulatedE(algo, compressed) << econtConfig.dropLSB;  


      std::cout << "input value DAQ: " << fixed_adc
                <<  ", Expected TC energy: " << tc_energy
                << std::endl;

      // std::cout << "input value Daq : " << fixed_adc
      //           << " hgroc compressed : " << hgrocComp 
      //           << " decompressed from ECONT: " << decompressed
      //           << " compressed by ECONT: " << compressed_bc            
      //           << std::endl;

      iec++; // counter for total econts
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
