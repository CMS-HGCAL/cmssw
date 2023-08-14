#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/Run.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "DataFormats/Common/interface/View.h"
#include "CommonTools/Utils/interface/StringCutObjectSelector.h"
#include "DataFormats/GeometryVector/interface/GlobalPoint.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableConfigRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableConfig.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiCellChannelInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiPMTileInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiPMTileInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalElectronicsMappingTools.h"

#include <vector>
#include <iostream>

template <typename T1, typename T2>
  class DigiInfoTableProducer : public edm::stream::EDProducer<> {
 public:
  DigiInfoTableProducer(edm::ParameterSet const& params)
    : name_(params.getParameter<std::string>("name")),
    doc_(params.getParameter<std::string>("doc")),
    srcHits_(consumes<T1>(params.getParameter<edm::InputTag>("srcHits"))),
    srcDigis_(consumes<T2>(params.getParameter<edm::InputTag>("srcDigis"))),
    cut_(params.getParameter<std::string>("cut"), true),
    fixCalibChannel_(params.getParameter<bool>("fixCalibChannel")),
    moduleInfoToken_(esConsumes<HGCalCondSerializableModuleInfo,HGCalCondSerializableModuleInfoRcd,edm::Transition::BeginRun>()),
    siModuleInfoToken_(esConsumes<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd,edm::Transition::BeginRun>()),
    configToken_(esConsumes<HGCalCondSerializableConfig, HGCalCondSerializableConfigRcd, edm::Transition::BeginRun>())
  {
    produces<nanoaod::FlatTable>();
  }

  ~DigiInfoTableProducer() override {}

  bool virtual findMatch(const typename T1::value_type& hit, const typename T2::value_type& digi, const std::map<uint32_t,uint32_t>& info) {
    throw cms::Exception("DigiInfoTableProducer:findMatch") << "Virtual function findMatch is not implemented!";
  }

  uint32_t virtual findMatchEleId(const typename T1::value_type& hit, const std::map<uint32_t,uint32_t> & ele2geo_) {
    throw cms::Exception("DigiInfoTableProducer:findMatchEleId") << "Virtual function findMatchEleId is not implemented!";
  }

  uint32_t virtual getElecID(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:ElecID") << "Virtual function getElecID is not implemented!";
  }

  uint32_t virtual getEconDIdx(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:EconDIdx") << "Virtual function getEcondIdx is not implemented!";
  }

  uint32_t virtual getEconDeRx(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:EconDeRx") << "Virtual function getEcondeRx is not implemented!";
  }

  uint32_t virtual getHalfRocChannel(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:HalfRocChannel") << "Virtual function HalfRocChannel is not implemented!";
  }

  uint32_t virtual getElecIDisCM(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:ElecIDisCM") << "Virtual function ElecIDisCM is not implemented!";
  }

  float virtual getTOA(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:toa") << "Virtual function getTOA is not implemented!";
  }

  float virtual getTOT(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:tot") << "Virtual function getTOA is not implemented!";
  }

  float virtual getADC(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:adc") << "Virtual function getADC is not implemented!";
  }

  float virtual getADCm1(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:adcm1") << "Virtual function getADCm1 is not implemented!";
  }

  float virtual getTctp(const typename T2::value_type& digi) {
    throw cms::Exception("DigiInfoTableProducer:tctp") << "Virtual function getTctp is not implemented!";
  }

  void beginRun(edm::Run const& iRun, const edm::EventSetup& iSetup) override {

    auto moduleInfo = iSetup.getData(moduleInfoToken_);
    auto siCellInfo = iSetup.getData(siModuleInfoToken_);

    ele2geo_=hgcal::mapSiGeoToElectronics(moduleInfo,siCellInfo,false);
    geo2ele_=hgcal::mapSiGeoToElectronics(moduleInfo,siCellInfo,true);

    auto config = iSetup.getData(configToken_);
    charMode_ = config.moduleConfigs[0].charMode;  // FIXME: for now use module with electronicsId = 0 as placeholder
  }

  void produce(edm::Event& iEvent, const edm::EventSetup& iSetup) override {

    edm::Handle<T1> objs;
    iEvent.getByToken(srcHits_, objs);

    edm::Handle<T2> objdigis;
    iEvent.getByToken(srcDigis_, objdigis);

    std::vector<float> toavals;
    std::vector<float> totvals;
    std::vector<float> adcvals;
    std::vector<float> adcm1vals;
    std::vector<uint16_t> tctpvals;
    std::vector<uint32_t> eleIDvals;
    std::vector<uint32_t> econDIdxvals;
    std::vector<uint32_t> econDeRxvals;
    std::vector<uint32_t> halfrocChannelvals;
    std::vector<uint32_t> elecIDisCMvals;

    for (const auto& obj : *objs) {
      if (cut_(obj)) {
        auto it = geo2ele_.find(obj.detid());
        if (it != geo2ele_.end()) {
          auto eleID = it->second;
          auto it_digi = std::find_if(
              objdigis->begin(), objdigis->end(), [&eleID](const auto& digi) { return digi.id() == eleID; });
          if (it_digi != objdigis->end()) {
            const auto& objdigi = *it_digi;
            toavals.emplace_back(getTOA(objdigi));
            totvals.emplace_back(getTOT(objdigi));
            adcvals.emplace_back(getADC(objdigi));
            adcm1vals.emplace_back(getADCm1(objdigi));
            tctpvals.emplace_back(getTctp(objdigi));
            eleIDvals.emplace_back(getElecID(objdigi));
            econDIdxvals.emplace_back(getEconDIdx(objdigi));
            econDeRxvals.emplace_back(getEconDeRx(objdigi));
            halfrocChannelvals.emplace_back(getHalfRocChannel(objdigi));
            elecIDisCMvals.emplace_back(getElecIDisCM(objdigi));
          }
        }
      }
    }

    auto tab = std::make_unique<nanoaod::FlatTable>(toavals.size(), name_, false, true);
    tab->addColumn<float>("digi_toa", toavals, "digi time of arrival");
    tab->addColumn<float>("digi_tot", totvals, "digi time of threshould");
    tab->addColumn<float>("digi_adc", adcvals, "digi adc");
    tab->addColumn<float>("digi_adcm1", adcm1vals, "digi adc-1");
    tab->addColumn<uint16_t>("digi_tctp", tctpvals, "digi tctp");
    tab->addColumn<uint32_t>("id_eleID", eleIDvals, "electronic ID raw");
    tab->addColumn<uint32_t>("id_EconDIdx", econDIdxvals, "EconD Idx");
    tab->addColumn<uint32_t>("id_EconDeRx", econDeRxvals, "EconD eRx");
    tab->addColumn<uint32_t>("id_halfrocChannel", halfrocChannelvals, "halfrocChannel");
    tab->addColumn<uint32_t>("id_elecIDisCM", elecIDisCMvals, "electronic ID isCM");

    iEvent.put(std::move(tab));

  }

 protected:
  const std::string name_, doc_;
  const edm::EDGetTokenT<T1> srcHits_;
  const edm::EDGetTokenT<T2> srcDigis_;
  const StringCutObjectSelector<typename T1::value_type> cut_;
  const bool fixCalibChannel_;

  //tokens and record watches
  edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
  edm::ESGetToken<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd> siModuleInfoToken_;
  edm::ESGetToken<HGCalCondSerializableConfig, HGCalCondSerializableConfigRcd> configToken_;

  std::map<uint32_t,uint32_t> ele2geo_;
  std::map<uint32_t,uint32_t> geo2ele_;
  bool charMode_ = false;
};
