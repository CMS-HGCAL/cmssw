#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "EventFilter/HGCalRawToDigi/interface/HGCalUnpacker.h"

#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiCollections.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHostCollection.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableConfigRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableConfig.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"



class HGCalRawToDigi : public edm::stream::EDProducer<> {
public:
  explicit HGCalRawToDigi(const edm::ParameterSet&);

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void produce(edm::Event&, const edm::EventSetup&) override;
  void beginRun(edm::Run const&, edm::EventSetup const&) override;

  const edm::EDGetTokenT<FEDRawDataCollection> fedRawToken_;
  const edm::EDPutTokenT<HGCalFlaggedECONDInfoCollection> flaggedRawDataToken_;
  const edm::EDPutTokenT<HGCalElecDigiCollection> elecDigisToken_;
  const edm::EDPutTokenT<HGCalElecDigiCollection> elecCMsToken_;
  const edm::EDPutTokenT<hgcaldigi::HGCalDigiHostCollection> elecDigisSoAToken_;
  edm::ESWatcher<HGCalCondSerializableConfigRcd> configWatcher_;
  edm::ESGetToken<HGCalCondSerializableConfig,HGCalCondSerializableConfigRcd> configToken_;
  edm::ESWatcher<HGCalCondSerializableModuleInfoRcd> eleMapWatcher_;
  edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;

  HGCalCondSerializableModuleInfo::ERxBitPatternMap erxEnableBits_;
  std::map<uint16_t,uint16_t> fed2slink_;
  
  const std::vector<unsigned int> fedIds_;
  const unsigned int flaggedECONDMax_;
  const unsigned int numERxsInECOND_;
  HGCalUnpackerConfig unpackerConfig_;
  HGCalModuleConfig moduleConfig_; // current module
  std::unique_ptr<HGCalUnpacker> unpacker_; // remove the const here to initialize in begin run

  const bool fixCalibChannel_;
};

HGCalRawToDigi::HGCalRawToDigi(const edm::ParameterSet& iConfig)
    : fedRawToken_(consumes<FEDRawDataCollection>(iConfig.getParameter<edm::InputTag>("src"))),
      flaggedRawDataToken_(produces<HGCalFlaggedECONDInfoCollection>("UnpackerFlags")),
      elecDigisToken_(produces<HGCalElecDigiCollection>("DIGI")),
      elecCMsToken_(produces<HGCalElecDigiCollection>("CM")),
      elecDigisSoAToken_(produces<hgcaldigi::HGCalDigiHostCollection>()),
      configToken_(esConsumes<HGCalCondSerializableConfig,HGCalCondSerializableConfigRcd>(
          iConfig.getParameter<edm::ESInputTag>("config_label"))),
      moduleInfoToken_(esConsumes<HGCalCondSerializableModuleInfo,HGCalCondSerializableModuleInfoRcd,edm::Transition::BeginRun>(
              iConfig.getParameter<edm::ESInputTag>("module_info_label"))),
      fedIds_(iConfig.getParameter<std::vector<unsigned int> >("fedIds")),
      flaggedECONDMax_(iConfig.getParameter<unsigned int>("flaggedECONDMax")),
      numERxsInECOND_(iConfig.getParameter<unsigned int>("numERxsInECOND")),
      unpackerConfig_(HGCalUnpackerConfig{.sLinkBOE = iConfig.getParameter<unsigned int>("slinkBOE"),
                                          .cbHeaderMarker = iConfig.getParameter<unsigned int>("cbHeaderMarker"),
                                          .econdHeaderMarker = iConfig.getParameter<unsigned int>("econdHeaderMarker"),
                                          .payloadLengthMax = iConfig.getParameter<unsigned int>("payloadLengthMax"),
                                          .applyFWworkaround = iConfig.getParameter<bool>("applyFWworkaround")}),
      fixCalibChannel_(iConfig.getParameter<bool>("fixCalibChannel")) {}

void HGCalRawToDigi::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup){
  auto moduleInfo = iSetup.getData(moduleInfoToken_);
  std::tuple<uint16_t,uint8_t,uint8_t,uint8_t> denseIdxMax = moduleInfo.getMaxValuesForDenseIndex();  
  unpackerConfig_.sLinkCaptureBlockMax=std::get<1>(denseIdxMax);
  unpackerConfig_.captureBlockECONDMax=std::get<2>(denseIdxMax);
  unpackerConfig_.econdERXMax=std::get<3>(denseIdxMax);
  unpackerConfig_.erxChannelMax=37;
  unpackerConfig_.channelMax=std::get<0>(denseIdxMax)*std::get<1>(denseIdxMax)*std::get<2>(denseIdxMax)*std::get<3>(denseIdxMax)*37;
  unpackerConfig_.commonModeMax=std::get<0>(denseIdxMax)*std::get<1>(denseIdxMax)*std::get<2>(denseIdxMax)*std::get<3>(denseIdxMax)*2;
  unpacker_=std::unique_ptr<HGCalUnpacker>(new HGCalUnpacker(unpackerConfig_));
  erxEnableBits_=moduleInfo.getERxBitPattern();
  fed2slink_=moduleInfo.getFedToSlinkMap();
  LogDebug("HGCalRawToDigi::erxbits") << "eRx enabled bits" << std::endl;
  for(auto it : erxEnableBits_)
    LogDebug("HGCalRawToDigi::erxbits") << it.first << " = " << "0x" << std::hex << (uint32_t)(it.second) << std::dec << std::endl;
}
//
void HGCalRawToDigi::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

  // retrieve the FED raw data
  const auto& raw_data = iEvent.get(fedRawToken_);
  
  // retrieve configuration from YAML files
  if (configWatcher_.check(iSetup)) {
    auto conds = iSetup.getData(configToken_);
    size_t nmods = conds.moduleConfigs.size();
    edm::LogInfo("HGCalRawToDigi") << "Conditions retrieved for " << nmods << " modules:\n" << conds << std::endl;
    for (auto it : conds.moduleConfigs) { // loop over map module electronicsId -> HGCalModuleConfig
      HGCalModuleConfig moduleConfig(it.second);
      edm::LogInfo("HGCalRawToDigi") << "  Module " << it.first << ": charMode=" << moduleConfig.charMode << std::endl;
    }
    moduleConfig_ = conds.moduleConfigs[0]; // for now use module with electronicsId = 0 as placeholder
  } // else: use previously loaded module configuration

  // prepare the output
  HGCalFlaggedECONDInfoCollection flagged_econds;
  HGCalDigiCollection digis;
  HGCalElecDigiCollection elec_digis;
  HGCalElecDigiCollection elec_cms;
  std::vector<uint32_t> elecid;
  std::vector<uint8_t> tctp;
  std::vector<uint16_t> adcm1;
  std::vector<uint16_t> adc;
  std::vector<uint16_t> tot;
  std::vector<uint16_t> toa;
  std::vector<uint16_t> cm; 
  for (const auto& fed_id : fedIds_) {
    const auto& fed_data = raw_data.FEDData(fed_id);
    if (fed_data.size() == 0)
      continue;
    
    auto* ptr = fed_data.data();
    size_t fed_size = fed_data.size();
    std::vector<uint32_t> data_32bit;
    for (size_t i = 0; i < fed_size; i += 4){
      data_32bit.emplace_back( (((*(ptr + i) & 0xff) << 0) +
                                (((i + 1) < fed_size) ? ((*(ptr + i + 1) & 0xff) << 8) : 0) +
                                (((i + 2) < fed_size) ? ((*(ptr + i + 2) & 0xff) << 16) : 0) +
                                (((i + 3) < fed_size) ? ((*(ptr + i + 3) & 0xff) << 24) : 0))
                               );
        
      //preserve pseudo-endianness
      //data_32bit.emplace_back(
      //                        ((*(ptr + i) & 0xff) << 24) +
      //                        (((i + 1) < fed_size) ? ((*(ptr + i + 1) & 0xff) << 16) : 0) +
      //                        (((i + 2) < fed_size) ? ((*(ptr + i + 2) & 0xff) << 8) : 0) +
      //                        (((i + 3) < fed_size) ? ((*(ptr + i + 3) & 0xff) << 0) : 0) );
    }
    
    LogDebug("HGCalRawToDigi")
              << std::dec << "FED ID=" << fed_id
              << std::hex << " First words: 0x" << data_32bit[0] << " 0x" << data_32bit[1]  
              << std::dec << " Data size=" << fed_size;

    unpacker_->parseSLink(
        data_32bit,
        [this](uint16_t sLink, uint8_t captureBlock, uint8_t econd) {
          return this->erxEnableBits_[HGCalCondSerializableModuleInfo::erxBitPatternMapDenseIndex(
              sLink, captureBlock, econd, 0, 0)];
        },
        [this](uint16_t fedid) {
          if (auto it = this->fed2slink_.find(fedid); it != this->fed2slink_.end()) {
            return this->fed2slink_.at(fedid);
          } else {
            // FIXME: throw an error or return 0?
            return (uint16_t)0;
          }
        });

    auto channeldata = unpacker_->channelData();
    auto commonModeSum=unpacker_->commonModeSum();
    for (unsigned int i = 0; i < channeldata.size(); i++) {
      auto data = channeldata.at(i);
      const auto& id = data.id();
      auto idraw = id.raw();
      auto raw = data.raw();
      LogDebug("HGCalRawToDigi:produce") << "channel data, id=" << idraw << ", raw=" << raw;
      elec_digis.push_back(data);
      elecid.push_back(id.raw());
      tctp.push_back(data.tctp());
      // FIXME: in the current HGCROC the calib channels (=18) is always in characterization model; to be fixed in ROCv3b
      auto charMode = moduleConfig_.charMode || (fixCalibChannel_ && id.halfrocChannel() == 18);
      adcm1.push_back(data.adcm1(charMode));
      adc.push_back(data.adc(charMode));
      tot.push_back(data.tot(charMode));
      toa.push_back(data.toa());
      cm.push_back(commonModeSum.at(i));
    }

    auto commonmode = unpacker_->commonModeData();
    for (unsigned int i = 0; i < commonmode.size(); i++) {
      auto cm = commonmode.at(i);
      const auto& id = cm.id();
      auto idraw = id.raw();
      auto raw = cm.raw();
      LogDebug("HGCalRawToDigi:produce") << "common modes, id=" << idraw << ", raw=" << raw;
      elec_cms.push_back(cm);
    }

    //append flagged ECONDs
    flagged_econds.insert(flagged_econds.end(), unpacker_->flaggedECOND().begin(), unpacker_->flaggedECOND().end());
  }

  //auto elec_digis_soa = std::make_unique<hgcaldigi::HGCalDigiHostCollection>(elec_digis.size(), cms::alpakatools::host());
  hgcaldigi::HGCalDigiHostCollection elec_digis_soa(elec_digis.size(),cms::alpakatools::host());
  for (unsigned int i = 0; i < elecid.size(); i++) {
      elec_digis_soa.view()[i].electronicsId() = elecid.at(i);
      elec_digis_soa.view()[i].tctp() = tctp.at(i);
      elec_digis_soa.view()[i].adcm1() = adcm1.at(i);
      elec_digis_soa.view()[i].adc() = adc.at(i);
      elec_digis_soa.view()[i].tot() = tot.at(i);
      elec_digis_soa.view()[i].toa() = toa.at(i);
      elec_digis_soa.view()[i].cm() = cm.at(i);
      elec_digis_soa.view()[i].flags() = 0;
  }

  //check how many flagged ECOND-s we have
  if(!flagged_econds.empty()) {

    LogDebug("HGCalRawToDigi:produce") << " caught " << flagged_econds.size() << " ECON-D with poor quality flags";
    
    if (flagged_econds.size() > flaggedECONDMax_) {
      throw cms::Exception("HGCalRawToDigi:produce")
        << "Too many flagged ECON-Ds: " << flagged_econds.size() << " > " << flaggedECONDMax_ << ".";
    }

  }
  
  //put information to the event
  iEvent.emplace(flaggedRawDataToken_,std::move(flagged_econds));
  iEvent.emplace(elecDigisToken_, std::move(elec_digis));
  iEvent.emplace(elecCMsToken_, std::move(elec_cms));
  iEvent.emplace(elecDigisSoAToken_, std::move(elec_digis_soa));
}

//
void HGCalRawToDigi::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("src", edm::InputTag("rawDataCollector"));
  desc.add<unsigned int>("maxCaptureBlock", 1)->setComment("maximum number of capture blocks in one S-Link");
  desc.add<unsigned int>("cbHeaderMarker", 0x5f)->setComment("capture block reserved pattern");
  desc.add<unsigned int>("econdHeaderMarker", 0x154)->setComment("ECON-D header Marker pattern");
  desc.add<unsigned int>("slinkBOE", 0x55)->setComment("SLink BOE pattern");
  desc.add<unsigned int>("captureBlockECONDMax", 12)->setComment("maximum number of ECON-Ds in one capture block");
  desc.add<bool>("applyFWworkaround",false)->setComment("use to enable dealing with firmware features (e.g. repeated words)");
  desc.add<bool>("fixCalibChannel", true)->setComment("FIXME: always treat calib channels in characterization mode; to be fixed in ROCv3b");
  desc.add<unsigned int>("econdERXMax", 12)->setComment("maximum number of eRxs in one ECON-D");
  desc.add<unsigned int>("erxChannelMax", 37)->setComment("maximum number of channels in one eRx");
  desc.add<unsigned int>("payloadLengthMax", 469)->setComment("maximum length of payload length");
  desc.add<unsigned int>("channelMax", 7000000)->setComment("maximum number of channels unpacked");
  desc.add<unsigned int>("commonModeMax", 4000000)->setComment("maximum number of common modes unpacked");
  desc.add<unsigned int>("flaggedECONDMax", 200)->setComment("maximum number of flagged ECON-Ds");
  desc.add<std::vector<unsigned int> >("fedIds", {});
  desc.add<unsigned int>("numERxsInECOND", 12)->setComment("number of eRxs in each ECON-D payload");
  desc.add<edm::ESInputTag>("config_label", edm::ESInputTag(""))->setComment("label for HGCalConfigESSourceFromYAML reader");
  desc.add<edm::ESInputTag>("module_info_label", edm::ESInputTag(""))->setComment("label for HGCalModuleInfoESSource");
  descriptions.add("hgcalDigis", desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalRawToDigi);
