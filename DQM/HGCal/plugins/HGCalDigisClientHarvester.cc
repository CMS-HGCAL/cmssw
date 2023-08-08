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
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Framework/interface/ESWatcher.h"

//DQM
#include "DQMServices/Core/interface/DQMEDHarvester.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "DQMServices/Core/interface/MonitorElement.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"

#include <TString.h>
#include <TFile.h>
#include <TKey.h>

#include <fstream>
#include <iostream>

/**
   @short DQM harvester for the DIGI monitoring elements at the end of lumi section / run 
*/
class HGCalDigisClientHarvester: public DQMEDHarvester{

public:

  typedef HGCalCondSerializableModuleInfo::ModuleInfoKey_t MonitorKey_t;
  
  HGCalDigisClientHarvester(const edm::ParameterSet& ps);
  virtual ~HGCalDigisClientHarvester();
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

protected:
  
  void beginJob();
  void dqmEndLuminosityBlock(DQMStore::IBooker &, DQMStore::IGetter &, edm::LuminosityBlock const &, edm::EventSetup const&);
  void dqmEndJob(DQMStore::IBooker &, DQMStore::IGetter &) override; 

private:

  /**
     @short saves the level-0 calibration parameters from the pedestal profiles of different modules harvested
   */
  void export_calibration_parameters(std::map<HGCalElectronicsId,hgcal::CellStatistics> &);

  //location of the hex map templates
  std::string templateROOT_;

  //monitoring elements
  std::map<MonitorKey_t, MonitorElement*> hex_channelId, hex_hgcrocPin, hex_sicellPadId,
    hex_pedestal,hex_noise, hex_cmrho, hex_bxm1rho,
    p_coeffs;

  //module mapper stuff
  edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
  edm::ESWatcher<HGCalCondSerializableModuleInfoRcd> miWatcher_;
  std::map<MonitorKey_t,MonitorKey_t> module_keys_;

  //output file with pedestals etc.
  std::string level0CalibOut_;
};

//
HGCalDigisClientHarvester::HGCalDigisClientHarvester(const edm::ParameterSet& iConfig)
  : templateROOT_(iConfig.getParameter<std::string>("HexTemplateFile")),
    moduleInfoToken_(esConsumes<HGCalCondSerializableModuleInfo,HGCalCondSerializableModuleInfoRcd,edm::Transition::EndLuminosityBlock>(iConfig.getParameter<edm::ESInputTag>("ModuleMapping"))),
    level0CalibOut_(iConfig.getParameter<std::string>("Level0CalibOut"))
{
}

//
void HGCalDigisClientHarvester::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::ESInputTag>("ModuleMapping",edm::ESInputTag(""));
  desc.add<std::string>("HexTemplateFile","/eos/cms/store/group/dpg_hgcal/comm_hgcal/ykao/hexagons_20230801.root");
  desc.add<std::string>("Level0CalibOut","level0_calibration_parameters.txt");
}


//
HGCalDigisClientHarvester::~HGCalDigisClientHarvester()
{
  edm::LogInfo("HGCalDigisClientHarvester") <<  "@ DTOR";
}

//
void HGCalDigisClientHarvester::beginJob()
{
  edm::LogInfo("HGCalDigisClientHarvester") << " @ beginJob";
}

//
void HGCalDigisClientHarvester::dqmEndLuminosityBlock(DQMStore::IBooker & ibooker, DQMStore::IGetter & igetter, edm::LuminosityBlock const & iLumi, edm::EventSetup const& iSetup) 
{

  //book histos only if module info changed (should happen end of first lumi section)
  if (!miWatcher_.check(iSetup)) return;

  // map with key=globalId, value=HGCROC_pin, "CALIB" is replaced with the same pin number as the companion cell
  std::map<int, int> map_HGCROC_pin = {{0,0}, {1,1}, {2,2}, {3,3}, {4,4}, {5,5}, {6,6}, {7,7}, {9,9}, {10,10}, {11,11}, {12,12}, {13,13}, {14,14}, {15,15}, {16,16}, {20,19}, {21,20}, {22,21}, {23,22}, {24,23}, {25,24}, {26,25}, {27,26}, {29,28}, {30,29}, {31,30}, {32,31}, {33,32}, {34,33}, {35,34}, {36,35}, {39,36}, {40,37}, {41,38}, {42,39}, {43,40}, {44,41}, {45,42}, {46,43}, {48,45}, {49,46}, {50,47}, {51,48}, {52,49}, {53,50}, {54,51}, {55,52}, {59,55}, {60,56}, {61,57}, {62,58}, {63,59}, {64,60}, {65,61}, {66,62}, {68,64}, {69,65}, {70,66}, {71,67}, {72,68}, {73,69}, {74,70}, {75,71}, {78,0}, {79,1}, {80,2}, {81,3}, {82,4}, {83,5}, {84,6}, {85,7}, {87,9}, {88,10}, {89,11}, {90,12}, {91,13}, {92,14}, {93,15}, {94,16}, {98,19}, {99,20}, {100,21}, {101,22}, {102,23}, {103,24}, {104,25}, {105,26}, {107,28}, {108,29}, {109,30}, {110,31}, {111,32}, {112,33}, {113,34}, {114,35}, {117,36}, {118,37}, {119,38}, {120,39}, {121,40}, {122,41}, {123,42}, {124,43}, {126,45}, {127,46}, {128,47}, {129,48}, {130,49}, {131,50}, {132,51}, {133,52}, {137,55}, {138,56}, {139,57}, {140,58}, {141,59}, {142,60}, {143,61}, {144,62}, {146,64}, {147,65}, {148,66}, {149,67}, {150,68}, {151,69}, {152,70}, {153,71}, {156,0}, {157,1}, {158,2}, {159,3}, {160,4}, {161,5}, {162,6}, {163,7}, {165,9}, {166,10}, {167,11}, {168,12}, {169,13}, {170,14}, {171,15}, {172,16}, {176,19}, {177,20}, {178,21}, {179,22}, {180,23}, {181,24}, {182,25}, {183,26}, {185,28}, {186,29}, {187,30}, {188,31}, {189,32}, {190,33}, {191,34}, {192,35}, {195,36}, {196,37}, {197,38}, {198,39}, {199,40}, {200,41}, {201,42}, {202,43}, {204,45}, {205,46}, {206,47}, {207,48}, {208,49}, {209,50}, {210,51}, {211,52}, {215,55}, {216,56}, {217,57}, {218,58}, {219,59}, {220,60}, {221,61}, {222,62}, {224,64}, {225,65}, {226,66}, {227,67}, {228,68}, {229,69}, {230,70}, {231,71}, {18,12}, {57,55}, {96,12}, {135,55}, {174,12}, {213,55} };

  // map with key=globalId, value=padId
  std::map<int, int> map_SiCell_padId = {{0,36}, {1,26}, {2,35}, {3,25}, {4,8}, {5,17}, {6,16}, {7,7}, {9,6}, {10,15}, {11,5}, {12,13}, {13,34}, {14,33}, {15,23}, {16,24}, {18,14}, {20,4}, {21,12}, {22,3}, {23,11}, {24,2}, {25,1}, {26,10}, {27,9}, {29,21}, {30,31}, {31,22}, {32,32}, {33,19}, {34,29}, {35,20}, {36,30}, {39,46}, {40,58}, {41,47}, {42,59}, {43,44}, {44,57}, {45,56}, {46,45}, {48,74}, {49,88}, {50,87}, {51,73}, {52,71}, {53,86}, {54,72}, {55,85}, {57,70}, {59,69}, {60,84}, {61,68}, {62,83}, {63,43}, {64,42}, {65,55}, {66,54}, {68,67}, {69,82}, {70,66}, {71,81}, {72,52}, {73,41}, {74,53}, {75,40}, {78,165}, {79,177}, {80,151}, {81,166}, {82,178}, {83,188}, {84,198}, {85,189}, {87,179}, {88,167}, {89,168}, {90,153}, {91,137}, {92,123}, {93,138}, {94,152}, {96,154}, {98,155}, {99,139}, {100,140}, {101,125}, {102,126}, {103,111}, {104,110}, {105,95}, {107,109}, {108,93}, {109,124}, {110,108}, {111,94}, {112,79}, {113,80}, {114,65}, {117,121}, {118,135}, {119,136}, {120,150}, {121,107}, {122,106}, {123,91}, {124,122}, {126,120}, {127,104}, {128,89}, {129,105}, {130,76}, {131,75}, {132,90}, {133,60}, {135,62}, {137,61}, {138,48}, {139,49}, {140,37}, {141,92}, {142,78}, {143,77}, {144,63}, {146,38}, {147,27}, {148,28}, {149,18}, {150,39}, {151,64}, {152,50}, {153,51}, {156,99}, {157,98}, {158,115}, {159,114}, {160,96}, {161,97}, {162,113}, {163,112}, {165,127}, {166,128}, {167,141}, {168,142}, {169,130}, {170,145}, {171,144}, {172,129}, {174,143}, {176,157}, {177,156}, {178,169}, {179,170}, {180,180}, {181,190}, {182,181}, {183,191}, {185,171}, {186,172}, {187,158}, {188,159}, {189,192}, {190,193}, {191,182}, {192,183}, {195,100}, {196,101}, {197,116}, {198,117}, {199,146}, {200,132}, {201,147}, {202,131}, {204,102}, {205,103}, {206,119}, {207,118}, {208,148}, {209,134}, {210,133}, {211,149}, {213,163}, {215,162}, {216,164}, {217,175}, {218,176}, {219,160}, {220,173}, {221,161}, {222,174}, {224,186}, {225,187}, {226,196}, {227,197}, {228,195}, {229,184}, {230,185}, {231,194}};
  
  //read module mapper and build list of module tags to retrieve the monitoring elements for
  auto moduleInfo = iSetup.getData(moduleInfoToken_);
  module_keys_ = moduleInfo.getAsSimplifiedModuleLocatorMap(true);
  for(auto m : moduleInfo.params_) {

    TString tag=Form("zside%d_plane%d_u%d_v%d",m.zside,m.plane,m.u,m.v);
    MonitorKey_t k(m.zside,m.plane,m.u,m.v);
    int nch(39*6*(1+m.isHD));

    ibooker.setCurrentFolder("HGCAL/Summary");    
    hex_channelId[k]   = ibooker.book2DPoly("hex_channelId_"   + tag, "LD wafer with global channel id (readout sequence); x[cm]; y[cm];ID", -26 , 26 , -28 , 24);
    hex_hgcrocPin[k]   = ibooker.book2DPoly("hex_hgcrocPin_"   + tag, "LD wafer with HGCROC pin/chan; x[cm]; y[cm];ID", -26 , 26 , -28 , 24);
    hex_sicellPadId[k] = ibooker.book2DPoly("hex_sicellPadId_" + tag, "LD wafer with Si cell pad Id; x[cm]; y[cm];ID", -26 , 26 , -28 , 24);
    hex_pedestal[k]    = ibooker.book2DPoly("hex_adc_avg_"     + tag, "; x[cm]; y[cm];Average ADC", -26 , 26 , -28 , 24);
    hex_noise[k]       = ibooker.book2DPoly("hex_adc_std_"     + tag, "; x[cm]; y[cm];ADC standard deviation", -26 , 26 , -28 , 24);
    hex_cmrho[k]       = ibooker.book2DPoly("hex_cmrho_"       + tag, "; x[cm]; y[cm];#rho(CM)", -26 , 26 , -28 , 24);
    hex_bxm1rho[k]     = ibooker.book2DPoly("hex_bxm1rho_"     + tag, "; x[cm]; y[cm];#rho(ADC_{-1})", -26 , 26 , -28 , 24);

    p_coeffs[k] = ibooker.book2D("coeffs_"+tag, ";Channel;", nch,0,nch, 11,0,11);
    p_coeffs[k]->setBinLabel(1,"<ADC>",2);
    p_coeffs[k]->setBinLabel(2,"#sigma(ADC)",2);
    p_coeffs[k]->setBinLabel(3,"#rho(ADC,CM)",2);
    p_coeffs[k]->setBinLabel(4,"k(ADC,CM)",2);
    p_coeffs[k]->setBinLabel(5,"o(ADC,CM)",2);
    p_coeffs[k]->setBinLabel(6,"#rho(ADC,ADC_{-1})",2);
    p_coeffs[k]->setBinLabel(7,"k(ADC,ADC_{-1})",2);
    p_coeffs[k]->setBinLabel(8,"o(ADC,ADC_{-1})",2);
    p_coeffs[k]->setBinLabel(9,"#rho(ADC,TDC)",2);
    p_coeffs[k]->setBinLabel(10,"k(ADC,TDC)",2);
    p_coeffs[k]->setBinLabel(11,"o(ADC,TDC)",2);   
  }

  //fill the 2DPoly from the hexmap external templates
  TFile *fgeo = new TFile(templateROOT_.c_str(), "R");
  TGraph *gr;
  TKey *key;
  TIter nextkey(fgeo->GetDirectory(nullptr)->GetListOfKeys());
  int i=0;
  while ((key = (TKey*)nextkey())) {
    TObject *obj = key->ReadObj();
    if(!obj->InheritsFrom("TGraph")) continue;
    gr = (TGraph*) obj;

    for(auto kit : hex_pedestal) { 
      MonitorKey_t k(kit.first);
      hex_channelId[k]->addBin(gr);
      hex_channelId[k]->setBinContent(i+1, i==0 ? 1e-6 : i);
      hex_hgcrocPin[k]->addBin(gr);
      hex_hgcrocPin[k]->setBinContent(i+1, (i==0||i==78||i==156) ? 1e-6 : map_HGCROC_pin[i]);
      hex_sicellPadId[k]->addBin(gr);
      hex_sicellPadId[k]->setBinContent(i+1, map_SiCell_padId[i]);
      hex_pedestal[k]->addBin(gr);
      hex_noise[k]->addBin(gr);
      hex_cmrho[k]->addBin(gr);
      hex_bxm1rho[k]->addBin(gr);
    }

    i++;    
  }
  fgeo->Close();

  edm::LogInfo("HGCalDigisClientHarvester") << "Retrieved : " << module_keys_.size() << " module tags to harvest and defined hexmaps";


  //loop over modules to harvest
  std::map<HGCalElectronicsId,hgcal::CellStatistics> summary_stats;
  for(auto it : module_keys_) {
    
    MonitorKey_t k(it.second);
    TString tag=Form("zside%d_plane%d_u%d_v%d",std::get<0>(k),std::get<1>(k),std::get<2>(k),std::get<3>(k));

    std::string meName("HGCAL/Digis/sums_"+tag);
    const MonitorElement *me = igetter.get(meName);
    if (me == nullptr) continue;
    
    //convert the sums to coefficients
    for(int ibin=1; ibin<me->getNbinsX()+1; ibin++) {
      hgcal::CellStatistics stats;
      ibin = ibin - 1; // somehow getBinContent starts from ibin = 0 ...
      stats.n      = me->getBinContent(ibin,1);
      stats.sum_x  = me->getBinContent(ibin,2);
      stats.sum_xx = me->getBinContent(ibin,3);
      stats.sum_s  = {me->getBinContent(ibin,4),me->getBinContent(ibin,7),me->getBinContent(ibin,10)};
      stats.sum_ss = {me->getBinContent(ibin,5),me->getBinContent(ibin,8),me->getBinContent(ibin,11)};
      stats.sum_xs = {me->getBinContent(ibin,6),me->getBinContent(ibin,9),me->getBinContent(ibin,12)};
      ibin = ibin + 1; // reset ibin
      
      std::pair<double,double> obs = stats.getObservableStats();
      std::vector<double> Rs = stats.getPearsonCorrelation();
      std::vector<double> slopes = stats.getSlopes();
      std::vector<double> intercepts = stats.getIntercepts();
      p_coeffs[k]->setBinContent(ibin,1,obs.first);
      p_coeffs[k]->setBinContent(ibin,2,obs.second);
      for(size_t i=0; i<3; i++) {
        p_coeffs[k]->setBinContent(ibin,3+3*i,Rs[i]);
        p_coeffs[k]->setBinContent(ibin,4+3*i,slopes[i]);
        p_coeffs[k]->setBinContent(ibin,5+3*i,intercepts[i]);
      }

      //fill hexplots
      hex_pedestal[k]->setBinContent(ibin,obs.first);
      hex_noise[k]->setBinContent(ibin,obs.second);
      hex_cmrho[k]->setBinContent(ibin,Rs[0]);
      hex_bxm1rho[k]->setBinContent(ibin,Rs[1]);
      
      //add to summary stats
      bool zside = std::get<0>(k);
      uint16_t slink = std::get<1>(k);
      uint16_t captureblock = std::get<2>(k);
      uint16_t econd = std::get<3>(k);
      uint16_t erx = (ibin-1)/39;
      uint16_t seq = (ibin-1)%39;
      HGCalElectronicsId eleid(zside,slink,captureblock,econd,erx,seq);      
      summary_stats[eleid]=stats;     
    }
    
  }

  //save pedestals to file
  export_calibration_parameters(summary_stats);
}

//
void HGCalDigisClientHarvester::export_calibration_parameters(std::map<HGCalElectronicsId,hgcal::CellStatistics> &stats) {

  //open txt file
  std::ofstream myfile(level0CalibOut_);

  myfile << "Channel Pedestal Noise CM_slope CM_offset BXm1_slope BXm1_offset\n";
  for(std::map<HGCalElectronicsId,hgcal::CellStatistics>::iterator it=stats.begin();
      it!=stats.end(); it++) {

    HGCalElectronicsId id=it->first;
    hgcal::CellStatistics s=it->second;
    std::pair<double,double> obs = s.getObservableStats();
    std::vector<double> slopes = s.getSlopes();
    std::vector<double> intercepts = s.getIntercepts();
    
    myfile << "0x" << std::hex << id.raw() << " " << std::dec
           << std::setprecision(3)
           << obs.first << " " << obs.second << " "
           << slopes[0] << " " << intercepts[0] << " "
           << slopes[1] << " " << intercepts[1] << " "
           << std::endl;
  }

  myfile.close();

  edm::LogInfo("HGCalDigisClient") << "Export CM parameters @ " << level0CalibOut_ << std::endl;
}

//
void HGCalDigisClientHarvester::dqmEndJob(DQMStore::IBooker& ibooker, DQMStore::IGetter& igetter)
{

}

#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(HGCalDigisClientHarvester);
