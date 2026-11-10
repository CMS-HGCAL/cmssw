#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "DQMServices/Core/interface/DQMEDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "DQMServices/Core/interface/MonitorElement.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiTriggerHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalECONTPacketInfoHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalFEDTriggerPacketInfoHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalRawDataDefinitions.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexerTrigger.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"
#include "DQM/HGCAL/interface/HGCalDQMCommon.h"

namespace {
  std::vector<int> getECONTErrorBins(uint16_t exceptionFlags) {
    std::vector<int> errorBins;
    if (hgcaldigi::hasStage1IOConversionError(exceptionFlags))
      errorBins.push_back(static_cast<int>(hgcal::dqm::EconTErrorType::NTC_NOT_MATCHING));
    if (hgcaldigi::hasWrongSubpacketHeader(exceptionFlags))
      errorBins.push_back(static_cast<int>(hgcal::dqm::EconTErrorType::SUBPACKET_ERROR));
    if (hgcaldigi::hastdaqIdxOutRange(exceptionFlags))
      errorBins.push_back(static_cast<int>(hgcal::dqm::EconTErrorType::TDAQIDX_OUT_RANGE));
    return errorBins;
  }
}  // namespace

/**
 * \class HGCalTPGDQM
 *
 * DQM client for HGCal trigger primitives. Reads trigger digis and ECON-T and
 * FED trigger packet info in SoA format and fills per-module TC occupancy and
 * energy, MS/total energy vs BX, and ECON-T header checks. It also fills ECON-T
 * exception quality per cassette, per FED and per layer, and resets the per-layer
 * histogram each lumisection. With SkipTriggerDQM (default true) it books and
 * fills nothing. Otherwise it processes the first MinimumEvents events and then
 * every PrescaleFactor-th event.
 */
class HGCalTPGDQM : public DQMEDAnalyzer {
public:
  struct TriggerMonitoredElement_t {
    std::string typecode;
    bool zside, isSiPM;
    uint32_t layer, i1, i2, nTrLinks, nTrCells, dqmIndex, fedid, modid, econtidx, cassette, endcap, moduleIndex,
        fedModuleIndex;
  };
  typedef std::pair<uint32_t, uint32_t> MonitoredElementKey_t;

  explicit HGCalTPGDQM(const edm::ParameterSet&);
  ~HGCalTPGDQM() override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void iterateEcontLSCounts(int layer, int flag);

  // ------------ member data ------------
  const edm::EDGetTokenT<hgcaldigi::HGCalDigiTriggerHost> digisTriggerTkn_;
  const edm::EDGetTokenT<hgcaldigi::HGCalECONTPacketInfoHost> econtInfoTkn_;
  const edm::EDGetTokenT<hgcaldigi::HGCalFEDTriggerPacketInfoHost> fedTriggerInfoTkn_;
  edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIdxBookingTkn_;
  edm::ESGetToken<HGCalMappingModuleIndexerTrigger, HGCalElectronicsMappingRcd> moduleIdxTriggerTkn_;
  edm::ESGetToken<HGCalMappingModuleIndexerTrigger, HGCalElectronicsMappingRcd> moduleIdxTriggerBookingTkn_;
  edm::ESGetToken<hgcal::HGCalMappingModuleParamHost, HGCalElectronicsMappingRcd> moduleInfoTkn_;
  edm::ESGetToken<hgcal::HGCalMappingModuleTriggerParamHost, HGCalElectronicsMappingRcd> moduleTriggerInfoTkn_;
  edm::ESGetToken<hgcal::HGCalDenseIndexTriggerInfoHost, HGCalDenseIndexInfoRcd> denseIndexTriggerInfoTkn_;

  const unsigned int minEvents_;
  const unsigned int prescaleFactor_;
  unsigned int nProcessed_;
  bool skipTriggerDQM_;

  std::map<MonitoredElementKey_t, TriggerMonitoredElement_t> followedTriggerModules_;
  std::map<uint32_t, std::vector<std::pair<MonitoredElementKey_t, TriggerMonitoredElement_t>>> triggerModulesByFED_;

  std::map<int, std::map<int, std::map<int, std::map<std::string, TriggerMonitoredElement_t>>>> HGCALTrigMap;

  std::map<std::string, std::map<MonitoredElementKey_t, MonitorElement*>> moduleTriggerHistos_;
  // endcap -> layer -> cassette -> ECON-T quality TH2 (read by HGCalTriggerWorker)
  std::map<int, std::map<int, std::map<int, MonitorElement*>>> econtQualityCassettes_;
  // per-FED ECON-T quality / BX0-from-Slink (not read by the harvester)
  std::map<uint32_t, MonitorElement*> econtQualityFEDs_;
  std::map<uint32_t, MonitorElement*> econtBx0_;
  // ECON-T exception vs directional layer, reset every LS (read by HGCalLSWorker)
  MonitorElement* me_econt_quality_layer = nullptr;
  std::set<int> unique_directional_layers_;
  edm::LuminosityBlockNumber_t currentLS_ = -1;

  std::vector<std::string> BXlist = {"BXm3", "BXm2", "BXm1", "BX0", "BXp1", "BXp2", "BXp3"};
  size_t nBX_ = BXlist.size();
  std::map<int, std::string> endCapKey = {{-1, "Minus"}, {1, "Plus"}};
};

//
// constructors and destructor
//
HGCalTPGDQM::HGCalTPGDQM(const edm::ParameterSet& iConfig)
    : digisTriggerTkn_(consumes<hgcaldigi::HGCalDigiTriggerHost>(iConfig.getParameter<edm::InputTag>("DigisTrigger"))),
      econtInfoTkn_(
          consumes<hgcaldigi::HGCalECONTPacketInfoHost>(iConfig.getParameter<edm::InputTag>("ECONTPacketInfo"))),
      fedTriggerInfoTkn_(consumes<hgcaldigi::HGCalFEDTriggerPacketInfoHost>(
          iConfig.getParameter<edm::InputTag>("FEDTriggerPacketInfo"))),
      moduleIdxBookingTkn_(esConsumes<edm::Transition::BeginRun>()),
      moduleIdxTriggerTkn_(esConsumes<edm::Transition::Event>()),
      moduleIdxTriggerBookingTkn_(esConsumes<edm::Transition::BeginRun>()),
      moduleInfoTkn_(esConsumes<edm::Transition::BeginRun>()),
      moduleTriggerInfoTkn_(esConsumes<edm::Transition::BeginRun>()),
      denseIndexTriggerInfoTkn_(esConsumes()),
      minEvents_(iConfig.getParameter<unsigned int>("MinimumEvents")),
      prescaleFactor_(std::max(1u, iConfig.getParameter<unsigned int>("PrescaleFactor"))),
      nProcessed_(0),
      skipTriggerDQM_(iConfig.getParameter<bool>("SkipTriggerDQM")) {}

HGCalTPGDQM::~HGCalTPGDQM() {}

//
// member functions
//

// ------------ method called for each event  ------------
void HGCalTPGDQM::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  ++nProcessed_;

  bool toProcess = (nProcessed_ < minEvents_) || (nProcessed_ % prescaleFactor_ == 0);
  if (!toProcess)
    return;

  if (skipTriggerDQM_)
    return;

  // Reset the per-LS ECON-T quality-vs-layer histogram on LS boundaries.
  edm::LuminosityBlockNumber_t lumi = iEvent.luminosityBlock();
  if (lumi != currentLS_ && me_econt_quality_layer != nullptr) {
    me_econt_quality_layer->Reset();
  }
  currentLS_ = lumi;

  const auto& econtInfoStage1 = iEvent.getHandle(econtInfoTkn_);
  if (econtInfoStage1.isValid()) {
    const auto& econtInfoStage1_view = econtInfoStage1->const_view();

    // Per-cassette / per-FED / per-layer ECON-T quality: fill (index, errorBin) for
    // each raised exception flag. Read from the same ECONTPacketInfo handle.
    for (const auto& [key, trigModule] : followedTriggerModules_) {
      const auto econt = econtInfoStage1_view[trigModule.dqmIndex];
      const auto errorBins = getECONTErrorBins(econt.exception());
      if (errorBins.empty())
        continue;

      MonitorElement* cassetteQualityH = nullptr;
      auto ec_it = econtQualityCassettes_.find(trigModule.endcap);
      if (ec_it != econtQualityCassettes_.end()) {
        auto lay_it = ec_it->second.find(trigModule.layer);
        if (lay_it != ec_it->second.end()) {
          auto cas_it = lay_it->second.find(trigModule.cassette);
          if (cas_it != lay_it->second.end())
            cassetteQualityH = cas_it->second;
        }
      }
      auto fedQ_it = econtQualityFEDs_.find(trigModule.fedid);
      const int directionalLayer = static_cast<int>(trigModule.layer) * static_cast<int>(trigModule.endcap);

      for (int errorID : errorBins) {
        if (cassetteQualityH != nullptr)
          cassetteQualityH->Fill(trigModule.moduleIndex, errorID);
        if (fedQ_it != econtQualityFEDs_.end())
          fedQ_it->second->Fill(trigModule.fedModuleIndex, errorID);
        iterateEcontLSCounts(directionalLayer, errorID);
      }
    }
  }

  edm::Handle<hgcaldigi::HGCalDigiTriggerHost> digisTrigger;
  if (!iEvent.getByToken(digisTriggerTkn_, digisTrigger))
    return;
  const auto& digisTrigger_view = digisTrigger->const_view();
  int32_t ndigisTrigger = digisTrigger_view.metadata().size();

  const auto& denseIndexInfo = iSetup.getData(denseIndexTriggerInfoTkn_);
  const auto& denseIndexInfo_view = denseIndexInfo.const_view();
  int ndiiTrigger = denseIndexInfo_view.metadata().size();
  if (ndiiTrigger != ndigisTrigger) {
    edm::LogWarning("hgcalSysValClient::analyzeTriggerModules")
        << "Number of digis and dense Index Info do not match: ndigis=" << ndigisTrigger << " ndii=" << ndiiTrigger
        << std::endl;
  }

  // FED trigger info (for counter2: ECON-T header vs FED BX from Slink).
  // skip if FED trigger info is unavailable
  const auto& fedTriggerInfo = iEvent.getHandle(fedTriggerInfoTkn_);
  const bool haveFedTriggerInfo = fedTriggerInfo.isValid();

  //loop to fill histograms
  typedef std::map<uint16_t, uint32_t> BxMSE;
  BxMSE bxmse;
  BxMSE bxmse_encoded;
  std::map<MonitoredElementKey_t, BxMSE> MSEArray;
  std::map<MonitoredElementKey_t, BxMSE> MSEArray_encoded;
  std::map<MonitoredElementKey_t, uint16_t> AlgoArray;
  std::vector<uint16_t> bxIdArray;
  int prevMod = -1;

  // Accumulate energies for ratio calculation: channel -> (central_sum, total_sum, total_count)
  std::map<MonitoredElementKey_t, std::map<uint16_t, std::tuple<uint32_t, uint32_t, uint32_t>>> tcEnergyStats;

  for (int32_t i = 0; i < ndigisTrigger; ++i) {
    auto digiInfo = denseIndexInfo_view[i];
    auto digiTrigger = digisTrigger_view[i];

    MonitoredElementKey_t key(digiInfo.fedId(), digiInfo.fedReadoutSeq());
    if (followedTriggerModules_.find(key) == followedTriggerModules_.end())
      continue;

    if (digiInfo.fedReadoutSeq() != prevMod) {
      bxIdArray.resize(0);
      bxmse.clear();
    }
    LogDebug("hgcalSysValClient::analyzeTriggerModules")
        << "itrig: " << i << ", digiInfo.fedReadoutSeq(): " << digiInfo.fedReadoutSeq() << std::endl;
    for (int ibx = 0; ibx < 7; ibx++) {
      if (digiTrigger.valid()(ibx, 0)) {
        LogDebug("hgcalSysValClient::analyzeTriggerModules")
            << ", ibx : " << ibx << ", econt header: " << uint16_t(digiTrigger.econtHeader()(ibx, 0))
            << ", digiTrigger.valid: " << digiTrigger.valid()(ibx, 0) << ", totE : " << digiTrigger.TotE()(ibx, 0)
            << std::endl;
        uint16_t bxId = uint16_t(digiTrigger.econtHeader()(ibx, 0));
        if (std::find(bxIdArray.begin(), bxIdArray.end(), bxId) == bxIdArray.end()) {
          bxmse[ibx] = digiTrigger.TotE()(ibx, 0);
          bxmse_encoded[ibx] = digiTrigger.encodedTotE()(ibx, 0);
          MSEArray[key] = bxmse;
          MSEArray_encoded[key] = bxmse_encoded;
          bxIdArray.push_back(bxId);
          AlgoArray[key] = uint16_t(digiTrigger.algo());
        }  //fill MS energy array

        uint32_t tcE = digiTrigger.TCEnergy()(ibx, 0);
        uint32_t enTcE = digiTrigger.encodedTCEnergy()(ibx, 0);
        uint16_t tcAdd = digiTrigger.TCAddress()(ibx, 0);
        uint16_t nBxs = static_cast<uint16_t>(digiTrigger.nBxs());
        uint16_t centraliBx =
            (nBxs - 1) /
            2;  //this is also Bx ReadoutWindow as defined in run${runnumber}_board160_configuration.yaml CommonReadout-->RxChannels-->ReadoutWindow
        uint16_t bxOffset = 3 - centraliBx;  //assuming maximum of 7bxs
        if (ibx == centraliBx)
          moduleTriggerHistos_["centtcenergy"][key]->Fill(tcE);
        moduleTriggerHistos_["tcoccupancy"][key]->Fill(tcAdd);
        moduleTriggerHistos_["tcenergy"][key]->Fill(tcE);
        moduleTriggerHistos_["encoded_tcenergy"][key]->Fill(enTcE);
        moduleTriggerHistos_["valid"][key]->Fill(ibx + bxOffset - centraliBx, tcAdd);
        moduleTriggerHistos_["energy"][key]->Fill(ibx + bxOffset - centraliBx, tcAdd, tcE);
        moduleTriggerHistos_["energy2"][key]->Fill(ibx + bxOffset - centraliBx, tcAdd, tcE * tcE);

        // ECON-T header vs expected header (from Slink trailer bx counter)
        if (ibx == centraliBx) {
          moduleTriggerHistos_["counter"][key]->Fill(digiTrigger.econtHeader()(centraliBx, 0),
                                                     digiTrigger.expEcontHeader()(centraliBx, 0));
        }
        if (haveFedTriggerInfo) {
          const auto& fedTriggerInfo_view = fedTriggerInfo->const_view();
          moduleTriggerHistos_["counter2"][key]->Fill(digiTrigger.econtHeader()(ibx, 0),
                                                      fedTriggerInfo_view[digiInfo.fedId()].FEDBX_trig());
          // ECON-T header 15/31 marks BX0. fill per-FED Bx0-from-Slink profile
          if (digiTrigger.econtHeader()(ibx, 0) == 15 || digiTrigger.econtHeader()(ibx, 0) == 31) {
            auto bx0It = econtBx0_.find(digiInfo.fedId());
            if (bx0It != econtBx0_.end())
              bx0It->second->Fill(digiTrigger.econTId(), fedTriggerInfo_view[digiInfo.fedId()].FEDBX_trig());
          }
        }
        // ECON-T header 15/31 marks BX0. fill bx0vsbx with signed BX offset
        const auto econtHead = digiTrigger.econtHeader()(ibx, 0);
        if (econtHead == 15 || econtHead == 31) {
          moduleTriggerHistos_["bx0vsbx"][key]->Fill(ibx + bxOffset - centraliBx);
        }

        // Fill TC Profile histograms as a function of TC Channel (Address)
        moduleTriggerHistos_["tcprofile"][key]->Fill(tcAdd, tcE);
        // Accumulate for ratio calculation
        std::get<1>(tcEnergyStats[key][tcAdd]) += tcE;  // Add to total sum
        std::get<2>(tcEnergyStats[key][tcAdd])++;       // Increment total count
        if (ibx == centraliBx) {
          moduleTriggerHistos_["tcprofile_centbx"][key]->Fill(tcAdd, tcE);
          std::get<0>(tcEnergyStats[key][tcAdd]) += tcE;  // Add to central sum
        }
      }  //valid bx
    }  //loop bx
    prevMod = digiInfo.fedReadoutSeq();
  }

  LogDebug("hgcalSysValClient::analyzeTriggerModules") << " MSEArray.size() = " << MSEArray.size() << std::endl;
  for (const auto& itMsE : MSEArray) {
    LogDebug("hgcalSysValClient::analyzeTriggerModules")
        << " MSEArray.second.size (nofBxs) = " << itMsE.second.size() << std::endl;
    uint16_t centraliBx =
        (itMsE.second.size() - 1) /
        2;  //this is also defined in run${runnumber}_board160_configuration.yaml CommonReadout-->RxChannels-->ReadoutWindow
    uint16_t bxOffset = 3 - centraliBx;  //assuming maximum of 7bxs
    bxmse = itMsE.second;
    for (const auto& itBxMsE : bxmse) {
      moduleTriggerHistos_["bxmsenergy"][itMsE.first]->Fill(itBxMsE.first + bxOffset - centraliBx, itBxMsE.second);
      moduleTriggerHistos_["msenergy"][itMsE.first]->Fill(itBxMsE.second);
      if (itBxMsE.first == centraliBx)
        moduleTriggerHistos_["centmsenergy"][itMsE.first]->Fill(itBxMsE.second);
    }
    moduleTriggerHistos_["algo"][itMsE.first]->Fill(AlgoArray[itMsE.first]);
  }  //loop over modules

  // Encoded MS/Total energy: mirror the MSEArray loop above using the encoded totals.
  for (const auto& itMsE : MSEArray_encoded) {
    uint16_t centraliBx = (itMsE.second.size() - 1) / 2;
    uint16_t bxOffset = 3 - centraliBx;
    for (const auto& itBxMsE : itMsE.second) {
      moduleTriggerHistos_["bxmsenergy_encoded"][itMsE.first]->Fill(itBxMsE.first + bxOffset - centraliBx,
                                                                    itBxMsE.second);
      moduleTriggerHistos_["msenergy_encoded"][itMsE.first]->Fill(itBxMsE.second);
    }
  }

  // Calculate and fill ratio profiles
  for (const auto& moduleData : tcEnergyStats) {
    auto key = moduleData.first;
    for (const auto& chData : moduleData.second) {
      uint16_t channel = chData.first;
      auto stats = chData.second;
      uint32_t centralSum = std::get<0>(stats);
      uint32_t totalSum = std::get<1>(stats);

      // Calculate ratio: central_sum / total_sum
      if (totalSum > 0) {
        double ratio = (double)centralSum / (double)totalSum;
        moduleTriggerHistos_["tcratio_profile"][key]->Fill(channel, ratio);
      }
    }
  }
}

// Adds an error flag to the ECON-T exception vs directional-layer plot (reset every LS).
void HGCalTPGDQM::iterateEcontLSCounts(int layer, int flag) {
  if (me_econt_quality_layer == nullptr)
    return;
  int layer_id = unique_directional_layers_.count(layer)
                     ? std::distance(unique_directional_layers_.begin(), unique_directional_layers_.find(layer))
                     : -1;
  me_econt_quality_layer->Fill(layer_id, flag);
}

void HGCalTPGDQM::bookHistograms(DQMStore::IBooker& ibook, edm::Run const& run, edm::EventSetup const& iSetup) {
  if (skipTriggerDQM_)
    return;

  const HGCalMappingModuleIndexer& moduleIndexer = iSetup.getData(moduleIdxBookingTkn_);
  const hgcal::HGCalMappingModuleParamHost& moduleInfo = iSetup.getData(moduleInfoTkn_);
  const HGCalMappingModuleIndexerTrigger& moduleIndexerTrigger = iSetup.getData(moduleIdxTriggerBookingTkn_);
  const hgcal::HGCalMappingModuleTriggerParamHost& moduleInfoTrigger = iSetup.getData(moduleTriggerInfoTkn_);

  // Take the physical location of each trigger module from the DAQ mapping, matched by
  // typecode: the trigger mapping does not always carry the right location.
  std::map<std::string, hgcal::dqm::HGCalDQMModule> detectorModulesByTypecode;
  for (auto const& mod : hgcal::dqm::readoutModules(moduleIndexer, moduleInfo))
    detectorModulesByTypecode[mod.typecode] = mod;

  for (const auto& [rawTypecode, fedData] : moduleIndexerTrigger.typecodeMap()) {
    const uint32_t fedid = fedData.first;
    const uint32_t imod = fedData.second;
    const uint32_t denseModIdx = moduleIndexerTrigger.getIndexForModule(fedid, imod);
    const auto& modInfo = moduleInfoTrigger.view()[denseModIdx];

    // Format typecode for use in map key
    std::string typecode = rawTypecode;
    std::replace(typecode.begin(), typecode.end(), '-', '_');

    TriggerMonitoredElement_t ele{};
    ele.dqmIndex = denseModIdx;
    ele.typecode = typecode;
    ele.nTrLinks = moduleIndexerTrigger.getNumTrLinks(fedid, imod);
    ele.nTrCells = moduleIndexerTrigger.getNumChannels(fedid, imod);
    ele.fedid = fedid;
    ele.modid = imod;
    ele.econtidx = modInfo.econtidx();
    const auto detectorModule = detectorModulesByTypecode.find(typecode);
    if (detectorModule != detectorModulesByTypecode.end()) {
      ele.zside = detectorModule->second.zside;
      ele.endcap = detectorModule->second.endcap;
      ele.isSiPM = detectorModule->second.isSiPM;
      ele.layer = detectorModule->second.layer;
      ele.i1 = detectorModule->second.i1;
      ele.i2 = detectorModule->second.i2;
      ele.cassette = detectorModule->second.cassette;
    } else {
      ele.zside = modInfo.zside();
      ele.endcap = ele.zside ? 1 : -1;
      ele.isSiPM = modInfo.isSiPM();
      ele.layer = modInfo.plane();
      ele.i1 = modInfo.i1();
      ele.i2 = modInfo.i2();
      ele.cassette = modInfo.cassette();
    }
    unique_directional_layers_.insert(ele.endcap * static_cast<int>(ele.layer));

    // Store in maps
    MonitoredElementKey_t key(fedid, imod);
    ele.fedModuleIndex = triggerModulesByFED_[fedid].size();
    triggerModulesByFED_[fedid].emplace_back(key, ele);
    followedTriggerModules_[key] = ele;

    // Build hierarchical HGCALMap: endcap -> layer -> cassette -> typecode
    auto& cassetteMap = HGCALTrigMap[ele.endcap][ele.layer][ele.cassette];
    cassetteMap[typecode] = ele;
    cassetteMap[typecode].moduleIndex = cassetteMap.size() - 1;
    followedTriggerModules_[key].moduleIndex = cassetteMap[typecode].moduleIndex;
    triggerModulesByFED_[fedid].back().second.moduleIndex = cassetteMap[typecode].moduleIndex;
  }

  // These are the digi level plots for the trigger.
  ibook.setCurrentFolder("HGCAL/Trigger/");

  // Trigger histograms
  for (const auto& it : followedTriggerModules_) {
    MonitoredElementKey_t key = it.first;
    TriggerMonitoredElement_t trigModule = it.second;

    std::string typecode = trigModule.typecode;
    int cassette = trigModule.cassette;
    int layer = trigModule.layer;
    int endcap = trigModule.endcap;
    std::string endCapStr = endCapKey[endcap];
    int u_coordinate = trigModule.i1;
    int v_coordinate = trigModule.i2;
    std::string uvStr = "(u" + std::to_string(u_coordinate) + "-v" + std::to_string(v_coordinate) + ") ";
    std::string plotFolder = "HGCAL/Trigger/Endcap_" + endCapStr + "/Layer_" + std::to_string(layer) + "/Cassette_" +
                             std::to_string(cassette) + "/" + uvStr + typecode;
    ibook.setCurrentFolder(plotFolder);

    int nTrCells = trigModule.nTrCells;

    std::ostringstream ss;
    ss << "_module_" << trigModule.dqmIndex;
    std::string tag(ss.str());

    int nBX = static_cast<int>(nBX_);

    ////////////////////////// Prior to TPG Unpacker installation ////////////////////////////////////////
    moduleTriggerHistos_["algo"][key] = ibook.book1D("algo", "Algo;Algo;Counts", 4, -0.5, 3.5);
    moduleTriggerHistos_["algo"][key]->setBinLabel(1, "BC");
    moduleTriggerHistos_["algo"][key]->setBinLabel(2, "STC4A(4E3M)");
    moduleTriggerHistos_["algo"][key]->setBinLabel(3, "STC4B(5E3M)");
    moduleTriggerHistos_["algo"][key]->setBinLabel(4, "STC16");

    moduleTriggerHistos_["valid"][key] = ibook.book2D(
        "occupancy", "Occupancy;BX;Trigger_Cell", nBX, -nBX / 2 - 0.5, nBX / 2 + 0.5, nTrCells, 0, nTrCells);
    moduleTriggerHistos_["energy"][key] = ibook.book2D(
        "BX_energy", "BX_energy;BX;Trigger_Cell", nBX, -nBX / 2 - 0.5, nBX / 2 + 0.5, nTrCells, 0, nTrCells);
    moduleTriggerHistos_["energy2"][key] = ibook.book2D(
        "BX_energy2", "BX_energy^2;BX;Trigger_Cell", nBX, -nBX / 2 - 0.5, nBX / 2 + 0.5, nTrCells, 0, nTrCells);
    /////////////////////////// Prior to TPG Unpacker installation ////////////////////////////////////////

    /////////////////////////// After the incoporation of TPG Unpacker ////////////////////////////////////////
    moduleTriggerHistos_["bxmsenergy"][key] = ibook.bookProfile("bxmsenergy",
                                                                "MS/Total Energy vs Bx;Bx;MS/Total Energy(count)",
                                                                nBX,
                                                                -nBX / 2 - 0.5,
                                                                nBX / 2 + 0.5,
                                                                0.,
                                                                10000.,
                                                                "i");
    moduleTriggerHistos_["bxmsenergy"][key]->getTProfile()->SetLineWidth(4);
    moduleTriggerHistos_["bxmsenergy_encoded"][key] =
        ibook.bookProfile("bxmsenergy_encoded",
                          "MS/Total Energy encoded vs Bx;Bx;MS/Total Energy encoded(count)",
                          nBX,
                          -nBX / 2 - 0.5,
                          nBX / 2 + 0.5,
                          0.,
                          10000.,
                          "i");
    moduleTriggerHistos_["bxmsenergy_encoded"][key]->getTProfile()->SetLineWidth(4);

    moduleTriggerHistos_["msenergy"][key] =
        ibook.book1D("msenergy", "MS/Total energy distribution;MS/Total energy;Counts", 500, 0, 10000);
    moduleTriggerHistos_["msenergy_encoded"][key] = ibook.book1D(
        "msenergy_encoded", "MS/Total energy encoded distribution;MS/Total energy encoded;Counts", 200, 0, 200);
    moduleTriggerHistos_["centmsenergy"][key] = ibook.book1D(
        "centmsenergy", "Central MS/Total energy distribution;MS/Total energy of central Bx;Counts", 1000, 0, 10000);

    moduleTriggerHistos_["tcoccupancy"][key] =
        ibook.book1D("tcoccupancy", "TC/STC occupancy;TC/STC channel;Counts", 48, -0.5, 47.5);
    moduleTriggerHistos_["tcenergy"][key] =
        ibook.book1D("tcenergy", "TC/STC energy distribution;TC/STC Energy;Counts", 1000, 0, 10000);
    moduleTriggerHistos_["centtcenergy"][key] = ibook.book1D(
        "centtcenergy", "Central TC/STC energy distribution;TC/STC Energy of central Bx;Counts", 1000, 0, 10000);
    moduleTriggerHistos_["encoded_tcenergy"][key] = ibook.book1D(
        "encoded_tcenergy", "TC/STC encoded energy distribution;TC/STC Energy encoded;Counts", 100, 0, 100);

    moduleTriggerHistos_["counter"][key] =
        ibook.book2D("counter", "; ECONT header; expected header Slink", 32, -0.5, 31.5, 32, -0.5, 31.5);
    moduleTriggerHistos_["counter2"][key] =
        ibook.book2D("counter2", "; ECONT header; BX Slink", 32, -0.5, 31.5, 4000, 0, 4000);
    moduleTriggerHistos_["bx0vsbx"][key] =
        ibook.book1D("bx0vsbx", " BX0 vs BX ;Bx;Counts", nBX, -nBX / 2 - 0.5, nBX / 2 + 0.5);

    // TC Profile histograms: Energy as a function of Trigger Channel (TC Address)
    moduleTriggerHistos_["tcprofile"][key] =
        ibook.bookProfile("tcprofile",
                          "TC Energy Profile vs Channel;TC Channel;Average TC Energy",
                          nTrCells,
                          -0.5,
                          nTrCells - 0.5,
                          0,
                          1000,
                          "s");

    // Profile for each BX: TC Energy vs Channel for central BX
    moduleTriggerHistos_["tcprofile_centbx"][key] =
        ibook.bookProfile("tcprofile_centbx",
                          "TC Energy Profile vs Channel (Central BX);TC Channel;Average TC Energy",
                          nTrCells,
                          -0.5,
                          nTrCells - 0.5,
                          0,
                          1000,
                          "s");

    // Ratio profile: Central BX / Other BX energy ratio as a function of channel
    moduleTriggerHistos_["tcratio_profile"][key] =
        ibook.bookProfile("tcratio_profile",
                          "TC Energy Ratio (Central BX / Other BX) vs Channel;TC Channel;Ratio",
                          nTrCells,
                          -0.5,
                          nTrCells - 0.5,
                          0,
                          10.0,
                          "s");

    /////////////////////////// After the incoporation of TPG Unpacker ////////////////////////////////////////
  }

  // Per-(endcap,layer,cassette) ECON-T quality TH2: x = ECON-T module, y = exception flag.
  // Consumed by HGCalTriggerWorker (looked up as econtQualityCassette_<cassette>).
  size_t necontFlags = hgcal::dqm::econdTFlags.size();
  for (const auto& [endcap, layerMap] : HGCALTrigMap) {
    std::string endCapString = endCapKey[endcap];
    std::string endcapFolder = std::string("HGCAL/Trigger/") + "Endcap_" + endCapString + "/";
    for (const auto& [layer, cassetteMap] : layerMap) {
      std::string layerFolder = endcapFolder + "Layer_" + std::to_string(layer) + "/";
      for (const auto& [cassette, econtMap] : cassetteMap) {
        std::string cassetteFolder = layerFolder + "Cassette_" + std::to_string(cassette) + "/";
        ibook.setCurrentFolder(cassetteFolder);
        auto* h = ibook.book2D("econtQualityCassette_" + std::to_string(cassette),
                               ";ECON-T;Exception;",
                               econtMap.size(),
                               0,
                               econtMap.size(),
                               necontFlags,
                               0,
                               necontFlags);
        hgcal::dqm::addBinLabels(hgcal::dqm::econdTFlags, h, 2);
        for (const auto& [typecode, trigModule] : econtMap) {
          h->setBinLabel(trigModule.moduleIndex + 1, trigModule.typecode, 1);
        }
        econtQualityCassettes_[endcap][layer][cassette] = h;
      }
    }
  }

  // Per-FED ECON-T quality / BX0-from-Slink profile / Stage-1 TC profile.
  // Displayed as booked; not read back by any harvester worker.
  ibook.setCurrentFolder("HGCAL/FED");
  for (const auto& [fedid, triggerModules] : triggerModulesByFED_) {
    if (triggerModules.empty())
      continue;
    econtBx0_[fedid] = ibook.bookProfile("econtBX0_" + std::to_string(fedid),
                                         ";ECON-T; Bx0 from Slink;",
                                         triggerModules.size(),
                                         0,
                                         triggerModules.size(),
                                         3564,
                                         1,
                                         3564,
                                         "s");
    econtQualityFEDs_[fedid] = ibook.book2D("econtQualityFED_" + std::to_string(fedid),
                                            ";ECON-T;Exception;",
                                            triggerModules.size(),
                                            0,
                                            triggerModules.size(),
                                            necontFlags,
                                            0,
                                            necontFlags);
    hgcal::dqm::addBinLabels(hgcal::dqm::econdTFlags, econtQualityFEDs_[fedid], 2);
    for (const auto& trigPair : triggerModules) {
      const auto& trigModule = trigPair.second;
      econtBx0_[fedid]->setBinLabel(trigModule.econtidx + 1, trigModule.typecode, 1);
      econtQualityFEDs_[fedid]->setBinLabel(trigModule.fedModuleIndex + 1, trigModule.typecode, 1);
    }
  }

  // ECON-T exception vs directional layer, reset every LS. Read by HGCalLSWorker.
  ibook.setCurrentFolder("HGCAL");
  int nLayers = static_cast<int>(unique_directional_layers_.size());
  std::vector<std::string> layer_labels;
  layer_labels.reserve(unique_directional_layers_.size());
  for (int n : unique_directional_layers_) {
    layer_labels.push_back(std::to_string(n));
  }
  me_econt_quality_layer =
      ibook.book2D("econt_lastLS", ";Layer;Exception;", nLayers, 0, nLayers, necontFlags, 0, necontFlags);
  hgcal::dqm::addBinLabels(layer_labels, me_econt_quality_layer, 1);
  hgcal::dqm::addBinLabels(hgcal::dqm::econdTFlags, me_econt_quality_layer, 2);
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void HGCalTPGDQM::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("DigisTrigger", edm::InputTag("hgcalDigisTrigger", ""));
  desc.add<edm::InputTag>("ECONTPacketInfo", edm::InputTag("hgcalDigisTrigger", ""));
  desc.add<edm::InputTag>("FEDTriggerPacketInfo", edm::InputTag("hgcalDigisTrigger", ""));
  desc.add<unsigned int>("MinimumEvents", 5000);
  desc.add<unsigned int>("PrescaleFactor", 5000);
  desc.add<bool>("SkipTriggerDQM", true);
  descriptions.add("hgcaltpgdqm", desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalTPGDQM);
