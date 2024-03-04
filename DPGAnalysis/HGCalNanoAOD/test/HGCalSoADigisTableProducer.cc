// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "CommonTools/Utils/interface/StringCutObjectSelector.h"

#include "DataFormats/NanoAOD/interface/FlatTable.h"

#include "DataFormats/HGCalDigi/interface/HGCalDigiHostCollection.h"
#include "DataFormats/HGCalRecHit/interface/HGCalRecHitHostCollection.h"
#include "DataFormats/HGCRecHit/interface/HGCRecHitCollections.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiCellChannelInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalElectronicsMappingTools.h"

#include <iostream>


//
class HGCalSoADigisTableProducer : public edm::stream::EDProducer<> {
public:

  explicit HGCalSoADigisTableProducer(const edm::ParameterSet& iConfig)
    : digisToken_(consumes<hgcaldigi::HGCalDigiHostCollection>(iConfig.getParameter<edm::InputTag>("Digis"))),
      rechitsToken_(consumes<hgcalrechit::HGCalRecHitHostCollection>(iConfig.getParameter<edm::InputTag>("RecHits"))),
      moduleInfoToken_(esConsumes<HGCalCondSerializableModuleInfo,HGCalCondSerializableModuleInfoRcd,edm::Transition::BeginRun>(iConfig.getParameter<edm::ESInputTag>("ModuleInfo"))),
      siModuleInfoToken_(esConsumes<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd,edm::Transition::BeginRun>(iConfig.getParameter<edm::ESInputTag>("SiModuleInfo")))
  {
    produces<nanoaod::FlatTable>("HGC");
  }
  
  ~HGCalSoADigisTableProducer() override {}

  /**
     given (u,v) returns the (x,y) coordinates of the cell
     isHD is used to choose the appropriate constants for HD / LD wafers
   */
  std::pair<float,float> simpleUVtoXY(int u, int v, bool isHD=false) {

    const float N=isHD ? 12 : 8; //wafer characteristic number (cells per side)
    const float R = 16.7441 / (3*N); //wafer flat-to-flat / 3N
    const float r = R*sqrt(3.)/2.; // R *sin(60)

    float x=-(1.5*(u-v)-0.5)*R;
    float y=(u+v-2*N+1)*r;
    
    float theta=-M_PI/3.;
    float xp=x*cos(theta)-y*sin(theta);
    float yp=x*sin(theta)+y*cos(theta);    

    return std::pair<float,float>(xp,yp);
  }
  
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("Digis",edm::InputTag("hgcalDigis"));
    desc.add<edm::InputTag>("RecHits",edm::InputTag("hgcalRecHit"));
    desc.add<edm::ESInputTag>("ModuleInfo",edm::ESInputTag(""));
    desc.add<edm::ESInputTag>("SiModuleInfo",edm::ESInputTag(""));
    descriptions.addWithDefaultLabel(desc);
  }

private:
  
  void beginStream(edm::StreamID) override {};
  
  void produce(edm::Event& iEvent, const edm::EventSetup& iSetup) override{
    
    using namespace edm;
    
    //retrieve digis
    const auto& digis = iEvent.get(digisToken_);
    auto const& digis_view = digis.const_view();
    int32_t ndigis=digis_view.metadata().size();

    //retrieve rechits and assert they have exactly the same size
    const auto& rechits = iEvent.get(rechitsToken_);
    auto const& rechits_view = rechits.const_view();
    int32_t nhits=rechits_view.metadata().size();
    assert(nhits==ndigis);

    //fill some extra info related to electronics/geometry
    std::vector<uint32_t> detid(ndigis);
    std::vector<uint8_t> layer(ndigis);
    std::vector<bool> zSide(ndigis),isCM(ndigis),isHD(ndigis);
    std::vector<uint16_t> fedId(ndigis),captureBlock(ndigis),econdIdx(ndigis),econdeRx(ndigis),halfrocChannel(ndigis),roc(ndigis),half(ndigis);
    std::vector<int16_t> waferU(ndigis),waferV(ndigis),chU(ndigis),chV(ndigis),chType(ndigis);
    std::vector<float> x(ndigis),y(ndigis);
  
    for(int32_t i = 0; i < nhits; ++i) {
      auto rh = rechits_view[i];

      //electronics id info
      HGCalElectronicsId eid( rh.detid() );
      zSide[i] = eid.zSide();
      fedId[i] = eid.fedId();
      captureBlock[i] = eid.captureBlock();
      econdIdx[i] = eid.econdIdx();
      econdeRx[i] = eid.econdeRx();
      roc[i] = econdeRx[i]/2;
      half[i] = econdeRx[i]%2;
      halfrocChannel[i] = eid.halfrocChannel();
      isCM[i] = eid.isCM();

      //read module info
      auto mi = moduleInfo_.getModule(eid);
      layer[i] = mi.plane;
      waferU[i] = mi.u;
      waferV[i] = mi.v;
      isHD[i] = mi.isHD;
      
      //read detailed si cell info
      uint32_t idx = ele2moduleinfo_[eid.raw()];
      auto sici = siCellInfo_.params_[idx];
      chU[i]=sici.iu;
      chV[i]=sici.iv;
      chType[i]=sici.t;
      if(chType[i]!=-1) {
        std::pair<float,float> xy=simpleUVtoXY(sici.iu,sici.iv,isHD[i]);
        x[i]=xy.first;
        y[i]=xy.second;
      } else {
        x[i]=-1;
        y[i]=-1;
      }
      
      //det id
      if(ele2detid_.count(eid.raw())!=1) continue;
      detid[i] = ele2detid_[eid.raw()];
    }
       
    //fill table
    auto out = std::make_unique<nanoaod::FlatTable>(ndigis ,"HGC",false);
    out->setDoc("HGC DIGIS and RecHits");
    out->addColumnFromArray<uint32_t>("eleid", digis_view.electronicsId(), "electronics id");
    out->addColumn<bool>("zSide", zSide, "z side");
    out->addColumn<uint16_t>("fedId", fedId, "FED index");
    out->addColumn<uint8_t>("captureBlock", captureBlock, "capture block index (with FED)");
    out->addColumn<uint8_t>("econdIdx", econdIdx, "ECON-D index (within capture block)");
    out->addColumn<uint8_t>("econdeRx", econdeRx, "ECON-D e-Rx (within ECON-D)");
    out->addColumn<uint8_t>("half", half, "HGCROC half");
    out->addColumn<uint8_t>("roc", roc, "roc");
    out->addColumn<uint8_t>("halfrocChannel", halfrocChannel, "1/2 ROC channel");
    out->addColumn<bool>("isCM", isCM, "is common mode");
    out->addColumn<bool>("isHD", isHD, "is high density");
    out->addColumn<uint8_t>("layer", layer, "layer");
    out->addColumn<int16_t>("waferU", waferU, "wafer U coordinate");
    out->addColumn<int16_t>("waferV", waferV, "wafer V coordinate");
    out->addColumn<int16_t>("chType", chType, "channel type");
    out->addColumn<int16_t>("u", chU, "channel U coordinate");
    out->addColumn<int16_t>("v", chV, "channel V coordinate");
    out->addColumn<float>("x", x, "x coordinate from standard tiling formula");
    out->addColumn<float>("y", y, "y coordinate from standard tiling formula");
    out->addColumn<uint32_t>("detid", detid, "detector id");
    out->addColumnFromArray<uint8_t>("tctp", digis_view.tctp(), "Tc/Tp flags (2b)");
    out->addColumnFromArray<uint16_t>("adc", digis_view.adc(), "adc measurement");
    out->addColumnFromArray<uint16_t>("adcm1", digis_view.adcm1(), "adc measurement in BX-1");
    out->addColumnFromArray<uint16_t>("tot", digis_view.tot(), "tot measurement");
    out->addColumnFromArray<uint16_t>("toa", digis_view.toa(), "toa measurement");
    out->addColumnFromArray<uint16_t>("flags", digis_view.flags(), "unpacking quality flags");
    out->addColumnFromArray<float>("energy", rechits_view.energy(), "calibrated energy");
    out->addColumnFromArray<float>("time", rechits_view.time(), "time");
    
    iEvent.put(std::move(out), "HGC");
  }
  
  void endStream() override {};
  
  void beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) override {
    
    //get module and silicon cell mapping
    moduleInfo_ = iSetup.getData(moduleInfoToken_);
    siCellInfo_ = iSetup.getData(siModuleInfoToken_);
    ele2detid_=hgcal::mapSiGeoToElectronics(moduleInfo_,siCellInfo_,false);
    ele2moduleinfo_=hgcal::mapSiElectronicsToChannelInfoIdx(moduleInfo_,siCellInfo_);
  }
  
  // ----------member data ---------------------------
  const edm::EDGetTokenT<hgcaldigi::HGCalDigiHostCollection> digisToken_;
  const edm::EDGetTokenT<hgcalrechit::HGCalRecHitHostCollection> rechitsToken_;
  const edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
  const edm::ESGetToken<HGCalCondSerializableSiCellChannelInfo,HGCalCondSerializableSiCellChannelInfoRcd> siModuleInfoToken_;  
  HGCalCondSerializableModuleInfo moduleInfo_;
  HGCalCondSerializableSiCellChannelInfo siCellInfo_;
  std::map<uint32_t,uint32_t> ele2detid_,ele2moduleinfo_;
};


//define this as a plug-in
DEFINE_FWK_MODULE(HGCalSoADigisTableProducer);
