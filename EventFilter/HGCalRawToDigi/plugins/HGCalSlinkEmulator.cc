/****************************************************************************
 *
 * This is a part of HGCAL offline software.
 * Authors:
 *   Pedro Silva, CERN
 *   Laurent Forthomme, CERN
 *
 ****************************************************************************/

#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/EmptyGroupDescription.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/CRC16.h"
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"
#include "DataFormats/FEDRawData/interface/FEDHeader.h"
#include "DataFormats/FEDRawData/interface/FEDTrailer.h"
#include "DataFormats/HGCalDigi/interface/HGCalRawDataEmulatorInfo.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalFrameGenerator.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalSlinkFromRaw.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableConfigRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableConfig.h"


class HGCalSlinkEmulator : public edm::stream::EDProducer<> {
public:
  explicit HGCalSlinkEmulator(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
private:
  void beginRun(edm::Run const&, edm::EventSetup const&) override;
  void produce(edm::Event&, const edm::EventSetup&) override;
  std::unique_ptr<FEDRawDataCollection> produceWithoutSlink(edm::Event& iEvent, const edm::EventSetup& iSetup);
  
  const unsigned int fed_id_;
  const bool store_emul_info_;
  const bool store_fed_header_trailer_;
  std::string emul_type_;
  const edm::EDPutTokenT<FEDRawDataCollection> fedRawToken_;
  const edm::EDPutTokenT<HGCalTestSystemMetaData> metadataToken_;
  edm::Service<edm::RandomNumberGenerator> rng_;
  edm::EDPutTokenT<HGCalSlinkEmulatorInfo> fedEmulInfoToken_;
  hgcal::HGCalFrameGenerator frame_gen_;
  std::unique_ptr<hgcal::SlinkFromRaw> raw_reader_;
  edm::ESGetToken<HGCalCondSerializableConfig,HGCalCondSerializableConfigRcd> configToken_;
  HGCalModuleConfig moduleConfig_;
};

//
HGCalSlinkEmulator::HGCalSlinkEmulator(const edm::ParameterSet& iConfig)
    : fed_id_(iConfig.getParameter<unsigned int>("fedId")),
      store_emul_info_(iConfig.getParameter<bool>("storeEmulatorInfo")),
      store_fed_header_trailer_(iConfig.getParameter<bool>("fedHeaderTrailer")),
      emul_type_(iConfig.getParameter<std::string>("emulatorType")),
      fedRawToken_(produces<FEDRawDataCollection>("hgcalFEDRawData")),
      metadataToken_(produces<HGCalTestSystemMetaData>("hgcalMetaData")),
      frame_gen_(iConfig),
      configToken_(esConsumes<HGCalCondSerializableConfig,HGCalCondSerializableConfigRcd,edm::Transition::BeginRun>(
                     iConfig.getParameter<edm::ESInputTag>("ModuleConfig")))
{
  
  if (emul_type_=="slinkfromraw") {
    raw_reader_ = std::make_unique<hgcal::SlinkFromRaw>(iConfig);
  } else {  
    frame_gen_.setEmulator(emul_type_);
    
    // ensure the random number generator service is present in configuration
    if (!rng_.isAvailable())
      throw cms::Exception("HGCalSlinkEmulator") << "The HGCalSlinkEmulator module requires the "
        "RandomNumberGeneratorService,\n"
        "which appears to be absent. Please add that service to your "
        "configuration\n"
        "or remove the modules that require it.";

    if (store_emul_info_)
      fedEmulInfoToken_ = produces<HGCalSlinkEmulatorInfo>();
  }

}

//
void HGCalSlinkEmulator::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup){
  auto conds = iSetup.getData(configToken_);
  moduleConfig_ = conds.moduleConfigs[0];
}


//
void HGCalSlinkEmulator::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  //produce raw / meta data
  std::unique_ptr<FEDRawDataCollection> raw_data;
  HGCalTestSystemMetaData meta_data;

  // try{
  if(emul_type_=="slinkfromraw") {
    raw_data = raw_reader_->next();
    meta_data = raw_reader_->nextMetaData();
  } else {
    raw_data = produceWithoutSlink(iEvent,iSetup);
    meta_data = frame_gen_.produceMetaData();
    
    // store the emulation information for events without real s-link, if requested
    if (store_emul_info_)
      iEvent.emplace(fedEmulInfoToken_, frame_gen_.lastSlinkEmulatedInfo());
    
  }
  // } catch(cms::Exception &e){
  //  LogDebug("HGCalSlinkEmulator::produce") << e.what();
  // }

  meta_data.injgain_ = moduleConfig_.injgain;
  meta_data.injcalib_ = moduleConfig_.injcalib;

  iEvent.emplace(fedRawToken_, std::move(*raw_data));
  iEvent.emplace(metadataToken_, std::move(meta_data));
}


//
std::unique_ptr<FEDRawDataCollection> HGCalSlinkEmulator::produceWithoutSlink(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  //otherwise generate a new frame
  frame_gen_.setRandomEngine(rng_->getEngine(iEvent.streamID()));

  // build the S-link payload
  auto slink_event = frame_gen_.produceSlinkEvent(fed_id_);
  const auto slink_event_size = slink_event.size() * sizeof(slink_event.at(0));

  // compute the total S-link payload size
  size_t total_event_size = slink_event_size;
  if (store_fed_header_trailer_){
    total_event_size += FEDHeader::length + FEDTrailer::length;
  }
  // fill the output FED raw data collection
  auto raw_data = std::make_unique<FEDRawDataCollection>();
  auto& fed_data = raw_data->FEDData(fed_id_);
  fed_data.resize(total_event_size);
  auto* ptr = fed_data.data();

  if (store_fed_header_trailer_) {
    const auto& last_event = frame_gen_.lastECONDEmulatedInput();
    const auto event_id = std::get<0>(last_event.first), bx_id = std::get<1>(last_event.first);
    int trg_type = 0;
    // compose 2*32-bit FED header word
    FEDHeader::set(ptr, trg_type, event_id, bx_id, fed_id_);
    LogDebug("HGCalSlinkEmulator").log([&](auto& log) {
      const FEDHeader hdr(ptr);
      log << "FED header: lvl1ID=" << hdr.lvl1ID() << ", bxID=" << hdr.bxID() << ", source ID=" << hdr.sourceID()
          << ".";
    });
    ptr += FEDHeader::length;
  }

  // insert ECON-D payload
  std::memcpy(ptr, slink_event.data(), slink_event_size);
  ptr += slink_event_size;
  LogDebug("HGCalSlinkEmulator") << "Wrote " << slink_event.size() << " 64-bit words = " << slink_event_size
                                 << " 8-bit words.";

  if (store_fed_header_trailer_) {
    // compose 2*32-bit FED trailer word
    FEDTrailer::set(ptr,
                    slink_event.size() + 2,
                    evf::compute_crc(reinterpret_cast<uint8_t*>(slink_event.data()), slink_event_size),
                    0,
                    0);
    LogDebug("HGCalSlinkEmulator").log([&](auto& log) {
      const FEDTrailer trl(ptr);
      log << "FED trailer: fragment length: " << trl.fragmentLength() << ", CRC=0x" << std::hex << trl.crc() << std::dec
          << ", status: " << trl.evtStatus() << ".";
    });
    ptr += FEDTrailer::length;
  }

  return raw_data;
}


//
void HGCalSlinkEmulator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  auto desc = hgcal::HGCalFrameGenerator::description();
  desc.ifValue(
          edm::ParameterDescription<std::string>("emulatorType", "trivial", true),
          // trivial emulator
          "trivial" >> edm::EmptyGroupDescription() or
              //slinkemulator
              "slinkfromraw" >>
                  (edm::ParameterDescription<std::vector<unsigned>>("fedIds", {}, false) and
                   edm::ParameterDescription<std::vector<std::string>>("inputs", {}, false) and
                   edm::ParameterDescription<std::vector<std::string>>("trig_inputs", {}, false)) or
              // test beam tree content
              "hgcmodule" >> (edm::ParameterDescription<std::string>("treeName", "hgcroc_rawdata/eventdata", false) and
                              edm::ParameterDescription<std::vector<std::string>>("inputs", {}, false)))
      ->setComment("emulator mode (trivial, or hgcmodule, or slinkfromraw)");
  desc.add<unsigned int>("fedId", 0)->setComment("FED number delivering the emulated frames");
  desc.add<bool>("fedHeaderTrailer", false)->setComment("also add FED header/trailer info");
  desc.add<bool>("storeEmulatorInfo", false)
      ->setComment("also append a 'truth' auxiliary info to the output event content");
  desc.add<edm::ESInputTag>("ModuleConfig", edm::ESInputTag(""))->setComment("label for HGCalConfigESSourceFromYAML reader");
  descriptions.add("hgcalEmulatedSlinkRawData", desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalSlinkEmulator);
