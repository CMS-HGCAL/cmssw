#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "DQM/HGCal/interface/CellStatistics.h"

#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <string>
#include <sstream>
#include <math.h>
#include <map>

//Framework
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Framework/interface/ESWatcher.h"

//DQM
#include "DQMServices/Core/interface/DQMEDHarvester.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "DQMServices/Core/interface/MonitorElement.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiCellChannelInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"

#include <TString.h>
#include <TFile.h>
#include <TKey.h>

#include <fstream>
#include <iostream>

/**
   @short DQM harvester for the DIGI monitoring elements at the end of lumi section / run 
*/
class HGCalDigisClientHarvester : public DQMEDHarvester {
public:
  typedef HGCalCondSerializableModuleInfo::ModuleInfoKey_t MonitorKey_t;

  HGCalDigisClientHarvester(const edm::ParameterSet &ps);
  virtual ~HGCalDigisClientHarvester();
  static void fillDescriptions(edm::ConfigurationDescriptions &descriptions);

protected:
  void beginJob();
  void dqmEndLuminosityBlock(DQMStore::IBooker &,
                             DQMStore::IGetter &,
                             edm::LuminosityBlock const &,
                             edm::EventSetup const &);
  void dqmEndJob(DQMStore::IBooker &, DQMStore::IGetter &) override;

private:
  /**
     @short saves the level-0 calibration parameters from the pedestal profiles of different modules harvested
   */
  void export_calibration_parameters(std::map<HGCalElectronicsId, hgcal::CellStatistics> &);

  //location of the hex map templates
  std::string templateROOT_;

  //monitoring elements
  std::map<MonitorKey_t, MonitorElement *> hex_channelId, hex_hgcrocPin, hex_sicellPadId, 
    hex_pedestal, hex_noise, hex_cmrho, hex_bxm1rho, 
    hex_toa_n, hex_toa_avg, hex_toa_std, hex_tot_avg, hex_tot_std, 
    p_coeffs;

  //module mapper stuff
  edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
  edm::ESGetToken<HGCalCondSerializableSiCellChannelInfo, HGCalCondSerializableSiCellChannelInfoRcd> siModuleInfoToken_;
  edm::ESWatcher<HGCalCondSerializableModuleInfoRcd> miWatcher_;

  //output file with pedestals etc.
  std::string level0CalibOut_,level0DeadChannelsOut_;
};

//
HGCalDigisClientHarvester::HGCalDigisClientHarvester(const edm::ParameterSet &iConfig)
    : templateROOT_(iConfig.getParameter<std::string>("HexTemplateFile")),
      moduleInfoToken_(esConsumes<HGCalCondSerializableModuleInfo,
                                  HGCalCondSerializableModuleInfoRcd,
                                  edm::Transition::EndLuminosityBlock>()),
      siModuleInfoToken_(esConsumes<HGCalCondSerializableSiCellChannelInfo,
                                    HGCalCondSerializableSiCellChannelInfoRcd,
                                    edm::Transition::EndLuminosityBlock>()),
      level0CalibOut_(iConfig.getParameter<std::string>("Level0CalibOut")),
      level0DeadChannelsOut_(iConfig.getParameter<std::string>("Level0DeadChannelsOut")) {

}

//
void HGCalDigisClientHarvester::fillDescriptions(edm::ConfigurationDescriptions &descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<std::string>("HexTemplateFile",
                        "/eos/cms/store/group/dpg_hgcal/comm_hgcal/ykao/geometry_$MODULETYPE_wafer_20230919.root");
  desc.add<std::string>("Level0CalibOut", "level0_calib_params.txt");
  desc.add<std::string>("Level0DeadChannelsOut", "level0_deadch.txt");
  descriptions.addWithDefaultLabel(desc);
}

//
HGCalDigisClientHarvester::~HGCalDigisClientHarvester() { edm::LogInfo("HGCalDigisClientHarvester") << "@ DTOR"; }

//
void HGCalDigisClientHarvester::beginJob() { edm::LogInfo("HGCalDigisClientHarvester") << " @ beginJob"; }

//
void HGCalDigisClientHarvester::dqmEndLuminosityBlock(DQMStore::IBooker &ibooker,
                                                      DQMStore::IGetter &igetter,
                                                      edm::LuminosityBlock const &iLumi,
                                                      edm::EventSetup const &iSetup) {
  //book histos only if module info changed (should happen end of first lumi section)
  if (!miWatcher_.check(iSetup))
    return;

  //read module mapper and build list of module tags to retrieve the monitoring elements for
  const auto &moduleInfo = iSetup.getData(moduleInfoToken_);
  const auto &siCellInfo = iSetup.getData(siModuleInfoToken_);
  const auto &module_keys = moduleInfo.getAsSimplifiedModuleLocatorMap(true);
  edm::LogInfo("HGCalDigisClientHarvester")
      << "Retrieved : " << module_keys.size() << " module tags to harvest and defined hexmaps";

  for (const auto &m : moduleInfo.params_) {
    TString module_type = TString::Format("%s_%d", m.isHD ? "HD" : "LD", m.wafType);
    TString tag = Form("zside%d_plane%d_u%d_v%d", m.zside, m.plane, m.u, m.v);
    MonitorKey_t k(m.zside, m.plane, m.u, m.v);
    int nch(39 * 6 * (1 + m.isHD));

    // clang-format off
    ibooker.setCurrentFolder("HGCAL/Summary");
    hex_channelId[k] = ibooker.book2DPoly(
        "hex_channelId_" + tag + "_" + module_type, module_type + " wafer with global channel id (readout sequence); x[cm]; y[cm];ID", -14, 14, -14, 14);
    hex_hgcrocPin[k] = ibooker.book2DPoly(
        "hex_hgcrocPin_" + tag + "_" + module_type, module_type + " wafer with HGCROC pin/chan; x[cm]; y[cm];ID", -14, 14, -14, 14);
    hex_sicellPadId[k] = ibooker.book2DPoly(
        "hex_sicellPadId_" + tag + "_" + module_type, module_type + " wafer with Si cell pad Id; x[cm]; y[cm];ID", -14, 14, -14, 14);
    hex_pedestal[k] = ibooker.book2DPoly("hex_adc_avg_" + tag + "_" + module_type, "; x[cm]; y[cm];Average ADC", -14, 14, -14, 14);
    hex_noise[k] = ibooker.book2DPoly("hex_adc_std_" + tag + "_" + module_type, "; x[cm]; y[cm];ADC standard deviation", -14, 14, -14, 14);
    hex_cmrho[k] = ibooker.book2DPoly("hex_cmrho_" + tag + "_" + module_type, "; x[cm]; y[cm];#rho(CM)", -14, 14, -14, 14);
    hex_bxm1rho[k] = ibooker.book2DPoly("hex_bxm1rho_" + tag + "_" + module_type, "; x[cm]; y[cm];#rho(ADC_{-1})", -14, 14, -14, 14);
    hex_toa_n[k]   = ibooker.book2DPoly("hex_toa_n_" + tag + "_" + module_type, "; x[cm]; y[cm];Hits with TOA", -14, 14, -14, 14);
    hex_toa_avg[k] = ibooker.book2DPoly("hex_toa_avg_" + tag + "_" + module_type, "; x[cm]; y[cm];Average TOA", -14, 14, -14, 14);
    hex_toa_std[k] = ibooker.book2DPoly("hex_toa_std_" + tag + "_" + module_type, "; x[cm]; y[cm];TOA standard deviation", -14, 14, -14, 14);
    hex_tot_avg[k] = ibooker.book2DPoly("hex_tot_avg_" + tag + "_" + module_type, "; x[cm]; y[cm];Average TOT", -14, 14, -14, 14);
    hex_tot_std[k] = ibooker.book2DPoly("hex_tot_std_" + tag + "_" + module_type, "; x[cm]; y[cm];TOT standard deviation", -14, 14, -14, 14);

    // clang-format on
    p_coeffs[k] = ibooker.book2D("coeffs_" + tag, ";Channel;", nch, 0, nch, 11, 0, 11);
    p_coeffs[k]->setBinLabel(1, "<ADC>", 2);
    p_coeffs[k]->setBinLabel(2, "#sigma(ADC)", 2);
    p_coeffs[k]->setBinLabel(3, "#rho(ADC,CM)", 2);
    p_coeffs[k]->setBinLabel(4, "k(ADC,CM)", 2);
    p_coeffs[k]->setBinLabel(5, "o(ADC,CM)", 2);
    p_coeffs[k]->setBinLabel(6, "#rho(ADC,ADC_{-1})", 2);
    p_coeffs[k]->setBinLabel(7, "k(ADC,ADC_{-1})", 2);
    p_coeffs[k]->setBinLabel(8, "o(ADC,ADC_{-1})", 2);
    p_coeffs[k]->setBinLabel(9, "#rho(ADC,TDC)", 2);
    p_coeffs[k]->setBinLabel(10, "k(ADC,TDC)", 2);
    p_coeffs[k]->setBinLabel(11, "o(ADC,TDC)", 2);

    const auto &siCells = siCellInfo.getAllCellsInModule(m.isHD, m.wafType);

    //fill the 2DPoly from the hexmap external templates
    std::ostringstream ss;
    ss << (m.isHD ? "HD" : "LD") << (m.wafType == 0 ? "" : std::to_string(m.wafType)) << "_"
       << (m.wafType == 0 ? "full" : "partial");
    TString hexmap_fn(templateROOT_.c_str());
    hexmap_fn.ReplaceAll("$MODULETYPE", ss.str());
    TFile *fgeo = new TFile(hexmap_fn, "R");
    TGraph *gr;
    TKey *key;
    TIter nextkey(fgeo->GetDirectory(nullptr)->GetListOfKeys());
    unsigned i = 0;
    auto clip = [](int i) -> double { return i > 0 ? i : 1e-6; };
    while ((key = (TKey *)nextkey())) {
      TObject *obj = key->ReadObj();
      if (!obj->InheritsFrom("TGraph"))
        continue;
      gr = (TGraph *)obj;

      unsigned idx = i - (i / 39) * 2;
      bool isCM = (i % 39 == 37) || (i % 39 == 38);

      hex_channelId[k]->addBin(gr);
      hex_channelId[k]->setBinContent(i + 1, clip(i));
      hex_hgcrocPin[k]->addBin(gr);
      hex_hgcrocPin[k]->setBinContent(i + 1, isCM ? 1e-6 : clip(siCells.at(idx).rocpin));
      hex_sicellPadId[k]->addBin(gr);
      hex_sicellPadId[k]->setBinContent(i + 1, isCM ? 1e-6 : clip(siCells.at(idx).sicell));
      hex_pedestal[k]->addBin(gr);
      hex_toa_n[k]->addBin(gr);
      hex_toa_avg[k]->addBin(gr);
      hex_toa_std[k]->addBin(gr);
      hex_tot_avg[k]->addBin(gr);
      hex_tot_std[k]->addBin(gr);
      hex_noise[k]->addBin(gr);
      hex_cmrho[k]->addBin(gr);
      hex_bxm1rho[k]->addBin(gr);

      i++;
    }
    fgeo->Close();
  }

  //loop over modules to harvest
  auto plane2eleidMap = moduleInfo.getAsSimplifiedModuleLocatorMap(false);
  std::map<HGCalElectronicsId, hgcal::CellStatistics> summary_stats;
  for (auto it : module_keys) {
    MonitorKey_t k(it.second);
    TString tag = Form("zside%d_plane%d_u%d_v%d", std::get<0>(k), std::get<1>(k), std::get<2>(k), std::get<3>(k));

    std::string meName("HGCAL/Digis/sums_" + tag);
    const MonitorElement *me = igetter.get(meName);
    if (me == nullptr)
      continue;

    //convert the sums to coefficients
    for (int ibin = 1; ibin <=me->getNbinsX(); ibin++) {
      hgcal::CellStatistics stats;
      ibin = ibin - 1;  // somehow getBinContent starts from ibin = 0 ...
      stats.n = me->getBinContent(ibin, 1);
      stats.sum_x = me->getBinContent(ibin, 2);
      stats.sum_xx = me->getBinContent(ibin, 3);
      stats.sum_s = {me->getBinContent(ibin, 4), me->getBinContent(ibin, 7), me->getBinContent(ibin, 10)};
      stats.sum_ss = {me->getBinContent(ibin, 5), me->getBinContent(ibin, 8), me->getBinContent(ibin, 11)};
      stats.sum_xs = {me->getBinContent(ibin, 6), me->getBinContent(ibin, 9), me->getBinContent(ibin, 12)};
      ibin = ibin + 1;  // reset ibin

      std::pair<double, double> obs = stats.getObservableStats();
      std::vector<double> Rs = stats.getPearsonCorrelation();
      std::vector<double> slopes = stats.getSlopes();
      std::vector<double> intercepts = stats.getIntercepts();
      p_coeffs[k]->setBinContent(ibin, 1, obs.first);
      p_coeffs[k]->setBinContent(ibin, 2, obs.second);
      for (size_t i = 0; i < 3; i++) {
        p_coeffs[k]->setBinContent(ibin, 3 + 3 * i, Rs[i]);
        p_coeffs[k]->setBinContent(ibin, 4 + 3 * i, slopes[i]);
        p_coeffs[k]->setBinContent(ibin, 5 + 3 * i, intercepts[i]);
      }

      //require some dispersion of values as this may be a dead channel
      //which will pollute the final summary
      if(obs.second>1e-3){
        hex_pedestal[k]->setBinContent(ibin, obs.first);
        hex_noise[k]->setBinContent(ibin, obs.second);
        hex_cmrho[k]->setBinContent(ibin, Rs[0]);
        hex_bxm1rho[k]->setBinContent(ibin, Rs[1]);
      }

      //add to summary stats using the logical id
      MonitorKey_t logik=plane2eleidMap[k];
      bool zside = std::get<0>(logik);
      uint16_t slink = std::get<1>(logik);
      uint16_t captureblock = std::get<2>(logik);
      uint16_t econd = std::get<3>(logik);
      uint16_t erx = (ibin - 1) / 39;
      uint16_t seq = (ibin - 1) % 39;
      HGCalElectronicsId eleid(zside, slink, captureblock, econd, erx, seq);
      summary_stats[eleid] = stats;

      //TOA
      hgcal::CellStatistics toastats;      
      toastats.n = me->getBinContent(ibin, 13);
      toastats.sum_x = me->getBinContent(ibin, 14);
      toastats.sum_xx = me->getBinContent(ibin, 15);
      std::pair<double, double> toaobs = toastats.getObservableStats();
      if(toaobs.second>1e-3){
	hex_toa_n[k]->setBinContent(ibin, toastats.n);
	hex_toa_avg[k]->setBinContent(ibin, toaobs.first);
	hex_toa_std[k]->setBinContent(ibin, toaobs.first);
      }

      //TOT
      hgcal::CellStatistics totstats;      
      totstats.n = me->getBinContent(ibin, 16);
      totstats.sum_x = me->getBinContent(ibin, 17);
      totstats.sum_xx = me->getBinContent(ibin, 18);
      std::pair<double, double> totobs = totstats.getObservableStats();
      if(totobs.second>1e-3){	
	hex_tot_avg[k]->setBinContent(ibin, totobs.first);
	hex_tot_std[k]->setBinContent(ibin, totobs.first);
      }
      
    }
  }

  //save pedestals to file
  export_calibration_parameters(summary_stats);
}

//
void HGCalDigisClientHarvester::export_calibration_parameters(
    std::map<HGCalElectronicsId, hgcal::CellStatistics> &stats) {

  //open txt files
  std::ofstream calibfile(level0CalibOut_);
  std::ofstream deadchfile(level0DeadChannelsOut_);

  calibfile << "Channel Pedestal Noise CM_slope CM_offset BXm1_slope BXm1_offset\n";
  for (std::map<HGCalElectronicsId, hgcal::CellStatistics>::iterator it = stats.begin(); it != stats.end(); it++) {
    HGCalElectronicsId id = it->first;
    hgcal::CellStatistics s = it->second;
    std::pair<double, double> obs = s.getObservableStats();
    std::vector<double> slopes = s.getSlopes();
    std::vector<double> intercepts = s.getIntercepts();

    calibfile << "0x" << std::setfill('0') << std::hex << id.raw() << " " << std::dec << std::setprecision(3) << obs.first << " " << obs.second
           << " " << slopes[0] << " " << intercepts[0] << " " << slopes[1] << " " << intercepts[1] << " " << std::endl;

    //very low noise is a candidate/dispersion of values is candidate for dead channel
    if(obs.second>1e-3) continue;
    deadchfile << "0x" << std::hex << std::setfill('0') << id.raw() << std::endl;    
  }

  calibfile.close();
  deadchfile.close();
  
  edm::LogInfo("HGCalDigisClient") << "Export level0 calibraiton parameters @ " << level0CalibOut_ << std::endl
                                   << "Dead channels @ " << level0DeadChannelsOut_ << std::endl;
}

//
void HGCalDigisClientHarvester::dqmEndJob(DQMStore::IBooker &ibooker, DQMStore::IGetter &igetter) {}

DEFINE_FWK_MODULE(HGCalDigisClientHarvester);
