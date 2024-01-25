// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "FWCore/PluginManager/interface/ModuleDef.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"

#include "SimCalorimetry/HGCalSimAlgos/interface/HGCalConditionsByAlgoWrapper.h"
#include "SimCalorimetry/HGCalSimAlgos/interface/HGCalSiSectionConfiguratorsByAlgo.h"

#include <TH1F.h>
#include <TH2F.h>
#include <TProfile.h>

#include <vector>
#include <sstream>
#include <string>
#include <iomanip>

#include "vdt/vdtMath.h"

using namespace std;

/**
 @class HGCalConditionsByAlgoAnalyzer
 @short an analyzer plugin to make basic histograms on the conditions/configurations to apply to sensors/front-end emulation
*/
class HGCalConditionsByAlgoAnalyzer : public edm::one::EDAnalyzer<edm::one::SharedResources> {

public:
  
  explicit HGCalConditionsByAlgoAnalyzer(const edm::ParameterSet &);
  ~HGCalConditionsByAlgoAnalyzer() override;

private:

  void beginJob() override {}
  void analyze(const edm::Event &, const edm::EventSetup &) override;

  /**
     @short this function takes care of profiling the Si parameterizations as function of the fluence
   */
  void profileSiParams();

  /**
     @short takes care of filling the geometry-dependent histograms
   */
  void fillSiHistograms(DetId::Detector &d, HGCSiConditionsByAlgoWrapper &conds);

  /**
     @short books all the histograms needed for the analysis
   */
  void bookHistograms(DetId::Detector d,unsigned int nlay,float xmin, float xmax, int xbins);
  void bookProfileHistograms();


  void endJob() override;
  
  // ----------member data ---------------------------
  const edm::ESGetToken<CaloGeometry, CaloGeometryRecord> tokGeom_;
  edm::Service<TFileService> fs_;

  HGCSiConditionsByAlgoWrapper hgceeConds_, hgchesiConds_;
  HGCSiPMonTileConditionsByAlgoWrapper hgcsciConds_;
  
  typedef std::pair<DetId::Detector,unsigned int> LayerKey_t;
  std::map<LayerKey_t,std::map<std::string, TH1F *> > histos1D_;
  
  const int plotMargin_ = 20;
};

//
HGCalConditionsByAlgoAnalyzer::HGCalConditionsByAlgoAnalyzer(const edm::ParameterSet &iConfig)
    : tokGeom_(esConsumes<CaloGeometry, CaloGeometryRecord>()) {
  usesResource("TFileService");
  fs_->file().cd();

  //common configuration parameters for the conditions by algo classes
  std::string doseMapURL(iConfig.getParameter<std::string>("doseMap"));
  double scaleByDoseFactor(iConfig.getParameter<double>("scaleByDoseFactor"));

  //conditions for the Si sections
  double ileakParam(
      iConfig.getParameter<edm::ParameterSet>("ileakParam").template getParameter<double>("ileakParam"));
  std::vector<double> cceParamFine(
      iConfig.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParamFine"));
  std::vector<double> cceParamThin(
      iConfig.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParamThin"));
  std::vector<double> cceParamThick(
      iConfig.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParamThick"));
  std::vector<unsigned int> confAlgo(iConfig.getParameter<std::vector<unsigned int> >("confAlgo"));

  hgceeConds_.getConditionsAlgo().setDoseMap(doseMapURL,confAlgo[0]);
  hgceeConds_.getConditionsAlgo().setFluenceScaleFactor(scaleByDoseFactor);
  hgceeConds_.getConditionsAlgo().setIleakParam(ileakParam*1e-19);
  hgceeConds_.getConditionsAlgo().setCceParam(HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t::HD120,cceParamFine);
  hgceeConds_.getConditionsAlgo().setCceParam(HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t::LD200,cceParamThin);
  hgceeConds_.getConditionsAlgo().setCceParam(HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t::LD300,cceParamThick);

  hgchesiConds_.getConditionsAlgo().setDoseMap(doseMapURL,confAlgo[1]);
  hgchesiConds_.getConditionsAlgo().setFluenceScaleFactor(scaleByDoseFactor);
  hgchesiConds_.getConditionsAlgo().setIleakParam(ileakParam*1e-19);
  hgchesiConds_.getConditionsAlgo().setCceParam(HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t::HD120,cceParamFine);
  hgchesiConds_.getConditionsAlgo().setCceParam(HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t::LD200,cceParamThin);
  hgchesiConds_.getConditionsAlgo().setCceParam(HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t::LD300,cceParamThick);

  //conditions for the SiPM-on-Tile section
  double refIdark = iConfig.getParameter<double>("referenceIdark");
  hgcsciConds_.getConditionsAlgo().setDoseMap(doseMapURL,confAlgo[2]);
  hgcsciConds_.getConditionsAlgo().setFluenceScaleFactor(scaleByDoseFactor);
  hgcsciConds_.getConditionsAlgo().setReferenceDarkCurrent(refIdark);

  //aimMIPtoADC_ = iConfig.getParameter<int>("aimMIPtoADC");
  //ignoreGainSettings_ = iConfig.getParameter<bool>("ignoreGainSettings");

  //get the errors correct
  TH1::SetDefaultSumw2();
}

//
void HGCalConditionsByAlgoAnalyzer::bookHistograms(DetId::Detector d,unsigned int nlay,float xmin, float xmax, int xbins) {


  TString title("CE-E");
  if(d == DetId::HGCalEE)  title="CE-H(Si)";
  if(d == DetId::HGCalHSc) title="CE-H(SiPM-on-tile)";
  
  for (unsigned int ilay = 1; ilay < nlay+1; ilay++) {

    LayerKey_t key(d,ilay);
     
    TString baseName(Form("d%d_layer%d_", d, ilay));
    TString baseTitle(Form("%s %d", title.Data(), ilay));
    baseTitle += ";Radius [cm];";
    histos1D_[key]["ncells"]  = fs_->make<TH1F>(baseName + "ncells",  baseTitle + "Cells",               xbins, xmin, xmax);
    //histos1D_[key]["noise"]   = fs_->make<TH1F>(baseName + "noise",   baseTitle + "<Noise> [fC]",        xbins, xmin, xmax);
    //histos1D_[key]["sn"]      = fs_->make<TH1F>(baseName + "sn",      baseTitle + "<S/N>",               xbins, xmin, xmax);
    histos1D_[key]["fluence"] = fs_->make<TH1F>(baseName + "fluence", baseTitle + "<F> [n_{eq}/cm^{2}]", xbins, xmin, xmax);
    
    if(d != DetId::HGCalHSc) {
      histos1D_[key]["cce"]   = fs_->make<TH1F>(baseName + "cce",   baseTitle + "<CCE>",             xbins, xmin, xmax);
      histos1D_[key]["mip"]   = fs_->make<TH1F>(baseName + "mip",   baseTitle + "<MIP> [fC]",             xbins, xmin, xmax);
      histos1D_[key]["ileak"] = fs_->make<TH1F>(baseName + "ileak", baseTitle + "<I_{leak}> [#muA]", xbins, xmin, xmax);
      //layerGain_[key] = fs_->make<TH1F>(baseName + "gain", baseTitle + "<Gain>", xbins, xmin, xmax);
      //layerMipPeak_[key] = fs_->make<TH1F>(baseName + "mippeak", layerTitle + "<MIP peak> [ADC]", xbins, xmin, xmax);
    }else{
      histos1D_[key]["dose"]   = fs_->make<TH1F>(baseName + "dose",   baseTitle + "<Dose> [kRad]", xbins, xmin, xmax);
    }
  }
}

//
void HGCalConditionsByAlgoAnalyzer::bookProfileHistograms() {
  
  float logfmin(13),logfmax(16.5);
  const int nfbins(200);
  Float_t fedges[nfbins+1]; 
  for(int i=0; i<=nfbins; i++) fedges[i]=pow(10,logfmin+(logfmax-logfmin)*i/nfbins);

  DetId::Detector dummy=(DetId::Detector)0;
  for(unsigned int sens=0; sens<4; sens++){

    TString baseName(Form("sensor%d", sens));

    TString baseTitle("HD 120#mum");
    if(sens==1) baseTitle = "LD 200#mum";
    if(sens==2) baseTitle = "LD 300#mum";
    if(sens==3) baseTitle = "HD 200#mum";
    
    LayerKey_t key(dummy,sens);
    histos1D_[key]["cce"] =fs_->make<TH1F>(baseName+"ccevsfluence", baseTitle + ";F [n_{eq}/cm^{2}];CCE",nfbins, fedges);
    histos1D_[key]["ileak"] =fs_->make<TH1F>(baseName+"ileakvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];I_{leak} [#muA]",nfbins, fedges);
    histos1D_[key]["mip"] =fs_->make<TH1F>(baseName+"mipvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];MIP_{eq} [fC]",nfbins, fedges);
    histos1D_[key]["gain"] =fs_->make<TH1F>(baseName+"gainvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];Dynamic range [fC]",nfbins, fedges);
    histos1D_[key]["encs"] =fs_->make<TH1F>(baseName+"encsvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];ENC_{s} [fC]",nfbins, fedges);
    histos1D_[key]["encp"] =fs_->make<TH1F>(baseName+"encpvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];ENC_{p} [fC]",nfbins, fedges);
    histos1D_[key]["enc"] =fs_->make<TH1F>(baseName+"encvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];ENC [fC]",nfbins, fedges);
    histos1D_[key]["son"] =fs_->make<TH1F>(baseName+"sonvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];S/N",nfbins, fedges);
    histos1D_[key]["son_up"] =fs_->make<TH1F>(baseName+"son_upvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];S/N",nfbins, fedges);
    histos1D_[key]["son_cm"] =fs_->make<TH1F>(baseName+"son_cmvsfluence", baseTitle + ";F [n_{eq}/cm^{2}];S/N",nfbins, fedges);
  }
  
}

//
void HGCalConditionsByAlgoAnalyzer::profileSiParams() {

  //prepare histos
  bookProfileHistograms();
  
  //loop over different sensor types and plot the main quantities as function of the fluence
  HGCROCEmulator<HGCROCChannelDataFrameSpec> roc;
  DetId::Detector dummy = (DetId::Detector)0;
  for(unsigned int i=0; i<4; i++){
    LayerKey_t key(dummy,i);

    //scan fluence based on the histogram axis
    HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t sens = (HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t)i;
    for(int ibin=1; ibin<=histos1D_[key]["cce"]->GetNbinsX()+1; ibin++) {

      double fluence = histos1D_[key]["cce"]->GetXaxis()->GetBinCenter(ibin);

      //si cell characteristics
      HGCalSiConditionsByAlgo::SiCellOpCharacteristics siop = hgceeConds_.getConditionsAlgo().getSiCellOpCharacteristics(fluence,sens);

      //front-end characteristics
      HGCROCConfiguration cfg=roc.proposeConfig(siop.mipEqfC);
      roc.setConfiguration(cfg);
      float encp=roc.getENCp(cfg.gain,siop.core.ileak);
      float encs=roc.getENCs(cfg.gain,siop.capacitance);
      float enc=sqrt(pow(encs,2)+pow(encp,2));
      float komega=1.2;  //enhancement from Omega measurements
      float enc_up=sqrt(pow(komega*encs,2)+pow(encp,2));
      float kcm=sqrt(1+1./2.);  //enhancement from CM subtraction with 2 channels
      float enc_cm=sqrt(pow(kcm*komega*encs,2)+pow(encp,2));

      //S/N
      float son=siop.mipEqfC/enc;
      float son_up=siop.mipEqfC/enc_up;
      float son_cm=siop.mipEqfC/enc_cm;

      //fill the histograms
      histos1D_[key]["cce"]->SetBinContent(ibin,siop.core.cce);
      histos1D_[key]["ileak"]->SetBinContent(ibin,siop.core.ileak);
      histos1D_[key]["mip"]->SetBinContent(ibin,siop.mipEqfC);
      histos1D_[key]["gain"]->SetBinContent(ibin,pow(2,int(cfg.gain))*80.);
      histos1D_[key]["encs"]->SetBinContent(ibin,encs);
      histos1D_[key]["encp"]->SetBinContent(ibin,encp);
      histos1D_[key]["enc"]->SetBinContent(ibin,enc);
      histos1D_[key]["son"]->SetBinContent(ibin,son);
      histos1D_[key]["son_up"]->SetBinContent(ibin,son_up);
      histos1D_[key]["son_cm"]->SetBinContent(ibin,son_cm);
    }
  }
}

//
void HGCalConditionsByAlgoAnalyzer::analyze(const edm::Event &iEvent, const edm::EventSetup &es) {
   
  //get geometry
  const auto &geom = es.getHandle(tokGeom_);

  //CE-E
  DetId::Detector det(DetId::HGCalEE);
  hgceeConds_.setGeometry(geom->getSubdetectorGeometry(det, ForwardSubdetector::ForwardEmpty));
  fillSiHistograms(det,hgceeConds_);

  //CE-H Si
  det=DetId::HGCalHSi;
  hgchesiConds_.setGeometry(geom->getSubdetectorGeometry(det, ForwardSubdetector::ForwardEmpty));
  fillSiHistograms(det,hgchesiConds_);

  //CE-H SiPM-on-Tile
  //hgcsciConds_.setGeometry(geom->getSubdetectorGeometry(DetId::HGCalHSc, ForwardSubdetector::ForwardEmpty));
  //fillSiPMonTileHistograms(det,hgcsciConds_);

  //generic profile
  profileSiParams();
}

//
void HGCalConditionsByAlgoAnalyzer::fillSiHistograms(DetId::Detector &det, HGCSiConditionsByAlgoWrapper &conds){
  
  //book histograms
  unsigned int nlay = conds.ddd()->layers(true);
  std::pair<double, double> ranZ = conds.ddd()->rangeZ(true);
  std::pair<double, double> ranRAtZ = conds.ddd()->rangeR(ranZ.first, true);
  bookHistograms(det,nlay,ranRAtZ.first-plotMargin_, ranRAtZ.second+plotMargin_,100);
  
  //loop over the available DetIds
  const std::vector<DetId> &detIdVec = conds.geom()->getValidDetIds();
  HGCalGeomSiSectionConfigurationByAlgo hgcsiConf;
  for (const auto &cellId : detIdVec) {

    double r = conds.computeRadius(cellId);

    //get the conditions either from cache or recomputing again from scratch
    HGCSiliconDetId id(cellId.rawId());
    int layer = id.layer();
    HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t sensType=(HGCalSiConditionsByAlgo::HGCalSiSensorTypes_t)id.type();
    
    HGCalSiConditionsByAlgo::SiCellOpCharacteristicsCore condsFromCache = conds.getConditionsForDetId(cellId);
    HGCalSiConditionsByAlgo::SiCellOpCharacteristics condsFromAlgo  = conds.getConditionsAlgo().getConditionsByAlgo(det,layer,r,sensType);

    //assert both values match
    assert(condsFromCache.cce == condsFromAlgo.core.cce);
    assert(condsFromCache.ileak == condsFromAlgo.core.ileak);

    //fill histos
    LayerKey_t key(det,layer);
    histos1D_[key]["ncells"]->Fill(r);
    histos1D_[key]["fluence"]->Fill(r,condsFromAlgo.fluence);
    histos1D_[key]["cce"]->Fill(r,condsFromAlgo.core.cce);
    histos1D_[key]["mip"]->Fill(r,condsFromAlgo.mipEqfC);
    histos1D_[key]["ileak"]->Fill(r,condsFromAlgo.core.ileak);

    hgcsiConf.addConfigurableToEntity(id,condsFromAlgo);
  }

  //determine best configuration
  hgcsiConf.findFEConfigurationByAlgo();

}


  /*
 

  */
  /*/
  std::vector<DetId::Detector> dets = {DetId::HGCalEE, DetId::HGCalHSi};
  for (const auto &d : dets) {
    noiseMaps_[d]->setGeometry(geom->getSubdetectorGeometry(d, ForwardSubdetector::ForwardEmpty));
    //sub-detector boundaries
    unsigned int nlay = noiseMaps_[d]->ddd()->layers(true);
    std::pair<double, double> ranZ = noiseMaps_[d]->ddd()->rangeZ(true);
    std::pair<double, double> ranRAtZ = noiseMaps_[d]->ddd()->rangeR(ranZ.first, true);
    std::pair<double, double> ranR(ranRAtZ.first - plotMargin_, ranRAtZ.second + plotMargin_);

    const std::vector<DetId> &detIdVec = noiseMaps_[d]->geom()->getValidDetIds();
    cout << "Subdetector:" << d << " has " << detIdVec.size() << " valid cells" << endl
         << "\t" << ranR.first << "<r<" << ranR.second << "\t" << ranZ.first << "<z<" << ranZ.second << endl;

    //start histos
    TString baseName(Form("d%d_", d));
    TString title(d == DetId::HGCalEE ? "CEE" : "CEH_{Si}");
    Int_t nbinsR(100);
    for (unsigned int ilay = 0; ilay < nlay; ilay++) {
      //this layer histos
      int layer(ilay + 1);
      std::pair<DetId::Detector, int> key(d, layer);
      TString layerBaseName(Form("%slayer%d_", baseName.Data(), layer));
      TString layerTitle(Form("%s %d", title.Data(), layer));
      layerTitle += ";Radius [cm];";
      layerN_[key] = fs_->make<TH1F>(layerBaseName + "ncells", layerTitle + "Cells", nbinsR, ranR.first, ranR.second);
      layerCCE_[key] = fs_->make<TH1F>(layerBaseName + "cce", layerTitle + "<CCE>", nbinsR, ranR.first, ranR.second);
      layerNoise_[key] =
          fs_->make<TH1F>(layerBaseName + "noise", layerTitle + "<Noise> [fC]", nbinsR, ranR.first, ranR.second);
      layerIleak_[key] =
          fs_->make<TH1F>(layerBaseName + "ileak", layerTitle + "<I_{leak}> [#muA]", nbinsR, ranR.first, ranR.second);
      layerSN_[key] = fs_->make<TH1F>(layerBaseName + "sn", layerTitle + "<S/N>", nbinsR, ranR.first, ranR.second);
      layerF_[key] = fs_->make<TH1F>(
          layerBaseName + "fluence", layerTitle + "<F> [n_{eq}/cm^{2}]", nbinsR, ranR.first, ranR.second);
      layerGain_[key] = fs_->make<TH1F>(layerBaseName + "gain", layerTitle + "<Gain>", nbinsR, ranR.first, ranR.second);
      layerMipPeak_[key] =
          fs_->make<TH1F>(layerBaseName + "mippeak", layerTitle + "<MIP peak> [ADC]", nbinsR, ranR.first, ranR.second);
    }

    //cce vs fluence
    for (unsigned int wafertype = 0; wafertype < (noiseMaps_[d]->getMipEqfC()).size(); ++wafertype) {
      std::pair<DetId::Detector, int> key2(d, wafertype);
      detCCEVsFluence_[key2] = fs_->make<TProfile>(
          baseName + Form("wt%d_", wafertype) + "cceVsFluence", title + ";<F> [n_{eq}/cm^{2}];<CCE>", 1000, 1e14, 1e16);
    }

    //sub-detector histos
    title += ";Layer;Radius [cm];";
    detN_[d] =
        fs_->make<TH2F>(baseName + "ncells", title + "Cells", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);
    detCCE_[d] = fs_->make<TH2F>(baseName + "cce", title + "<CCE>", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);
    detNoise_[d] =
        fs_->make<TH2F>(baseName + "noise", title + "<Noise> [fC]", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);
    detIleak_[d] = fs_->make<TH2F>(
        baseName + "ileak", title + "<I_{leak}> [#muA]", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);
    detSN_[d] = fs_->make<TH2F>(baseName + "sn", title + "<S/N>", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);
    detF_[d] = fs_->make<TH2F>(
        baseName + "fluence", title + "<F> [n_{eq}/cm^{2}]", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);
    detGain_[d] =
        fs_->make<TH2F>(baseName + "gain", title + "<Gain>", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);
    detMipPeak_[d] = fs_->make<TH2F>(
        baseName + "mippeak", title + "<MIP peak> [ADC]", nlay, 1, nlay + 1, nbinsR, ranR.first, ranR.second);

    //fill histos
    for (const auto &cellId : detIdVec) {
      HGCSiliconDetId id(cellId.rawId());
      int layer = std::abs(id.layer());
      GlobalPoint pt = noiseMaps_[d]->geom()->getPosition(id);
      double r(pt.perp());

      HGCalSiNoiseMap<HGCSiliconDetId>::GainRange_t gainToSet(HGCalSiNoiseMap<HGCSiliconDetId>::AUTO);
      if (ignoreGainSettings_)
        gainToSet = HGCalSiNoiseMap<HGCSiliconDetId>::q80fC;
      HGCalSiNoiseMap<HGCSiliconDetId>::SiCellOpCharacteristics siop =
          noiseMaps_[d]->getSiCellOpCharacteristics(id, gainToSet, aimMIPtoADC_);

      //fill histos (layer,radius)
      detN_[d]->Fill(layer, r, 1);
      detCCE_[d]->Fill(layer, r, siop.core.cce);
      detNoise_[d]->Fill(layer, r, siop.core.noise);
      detSN_[d]->Fill(layer, r, siop.mipfC / siop.core.noise);
      detIleak_[d]->Fill(layer, r, siop.ileak);
      detF_[d]->Fill(layer, r, siop.fluence);
      detGain_[d]->Fill(layer, r, siop.core.gain + 1);
      detMipPeak_[d]->Fill(layer, r, siop.mipADC);

      //per layer histograms
      std::pair<DetId::Detector, int> key(d, layer);
      layerN_[key]->Fill(r, 1);
      layerCCE_[key]->Fill(r, siop.core.cce);
      layerNoise_[key]->Fill(r, siop.core.noise);
      layerSN_[key]->Fill(r, siop.mipfC / siop.core.noise);
      layerIleak_[key]->Fill(r, siop.ileak);
      layerF_[key]->Fill(r, siop.fluence);
      layerGain_[key]->Fill(r, siop.core.gain + 1);
      layerMipPeak_[key]->Fill(r, siop.mipADC);

      std::pair<DetId::Detector, int> key2(d, id.type());
      detCCEVsFluence_[key2]->Fill(siop.fluence, siop.core.cce);
    }

    //normalize histos per cell counts
    detF_[d]->Divide(detN_[d]);
    detCCE_[d]->Divide(detN_[d]);
    detNoise_[d]->Divide(detN_[d]);
    detSN_[d]->Divide(detN_[d]);
    detIleak_[d]->Divide(detN_[d]);
    detGain_[d]->Divide(detN_[d]);
    detMipPeak_[d]->Divide(detN_[d]);
    for (unsigned int ilay = 0; ilay < nlay; ilay++) {
      int layer(ilay + 1);
      std::pair<DetId::Detector, int> key(d, layer);
      layerF_[key]->Divide(layerN_[key]);
      layerCCE_[key]->Divide(layerN_[key]);
      layerNoise_[key]->Divide(layerN_[key]);
      layerSN_[key]->Divide(layerN_[key]);
      layerIleak_[key]->Divide(layerN_[key]);
      layerGain_[key]->Divide(layerN_[key]);
      layerMipPeak_[key]->Divide(layerN_[key]);
    }
  }

  */


//
void HGCalConditionsByAlgoAnalyzer::endJob() {
  
  std::vector<std::string> hnames={"fluence","dose",
                                   "cce","mip","ileak","lysf","darkpx"};
  for(auto lk : histos1D_) {

    //the numerator needs to be present
    if(lk.second.count("ncells")==0) continue;
    
    for(auto name : hnames) {
      if(lk.second.count(name)==0) continue;
      lk.second[name]->Divide(lk.second["ncells"]);
    }
  }
  
}

//
HGCalConditionsByAlgoAnalyzer::~HGCalConditionsByAlgoAnalyzer() {}

//define this as a plug-in
DEFINE_FWK_MODULE(HGCalConditionsByAlgoAnalyzer);
