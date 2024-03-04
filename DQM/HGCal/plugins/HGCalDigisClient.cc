#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DQMServices/Core/interface/DQMEDAnalyzer.h"
#include "DQMServices/Core/interface/MonitorElement.h"

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "DataFormats/HGCalDigi/interface/HGCalTestSystemMetadata.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHostCollection.h"
#include "DataFormats/HGCalDigi/interface/HGCalFlaggedECONDInfo.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableSiCellChannelInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalElectronicsMappingTools.h"

#include <TString.h>

#include <string>
#include <fstream>
#include <iostream>
#include <map>

/**
   @short simple DQM client to monitor basic quantities at DIGI level
   @author Yu-Wei Kao
*/
class HGCalDigisClient : public DQMEDAnalyzer {
public:
  typedef HGCalCondSerializableModuleInfo::ModuleInfoKey_t MonitorKey_t;

  explicit HGCalDigisClient(const edm::ParameterSet&);
  ~HGCalDigisClient() override;

  /**
     @short method fills 'descriptions' with the allowed parameters for the module and their default values  
  */
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  /**
     @short takes care of booking the monitoring elements at the start of the run
     the histograms are instantiated per module according to the module mapping
     received from the event setup
   */
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;

  /**
     @short histogram filler per event
   */
  void analyze(const edm::Event&, const edm::EventSetup&) override;

  const edm::EDGetTokenT<hgcaldigi::HGCalDigiHostCollection> digisToken_;
  const edm::EDGetTokenT<HGCalFlaggedECONDInfoCollection> econdQualityToken_;
  const edm::EDGetTokenT<HGCalTestSystemMetaData> metadataToken_;

  std::map<MonitorKey_t, std::pair<int, double> > geomKey_maxADCpedsub;
  std::map<MonitorKey_t, MonitorElement*> p_hitcount, p_maxadcvstrigtime, p_adcvstrigtime, p_adcpedsubvstrigtime,
      p_totvstrigtime, p_toavstrigtime, p_adc, p_tot, p_adcm, p_toa, p_maxadc, p_sums, h_adc, h_adcm, h_tot, h_toa,
      h_adcpedsub;
  MonitorElement *h_trigtime;
  MonitorElement *p_econdquality, *p_econdcbquality, *p_econdpayload;
  std::map<MonitorKey_t, int> modbins_;

  // ------------ member data ------------
  edm::ESGetToken<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd> moduleInfoToken_;
  edm::ESGetToken<HGCalCondSerializableSiCellChannelInfo, HGCalCondSerializableSiCellChannelInfoRcd> siModuleInfoToken_;
  std::map<MonitorKey_t, MonitorKey_t> module_keys_;
  std::map<uint32_t, HGCalSiCellChannelInfo> eleidtoSiInfo_;
  const unsigned prescale_;
  const unsigned min_num_evts_;
  unsigned num_processed_ = 0;
};

//

HGCalDigisClient::HGCalDigisClient(const edm::ParameterSet& iConfig)
    : digisToken_(consumes<hgcaldigi::HGCalDigiHostCollection>(iConfig.getParameter<edm::InputTag>("Digis"))),
      econdQualityToken_(
          consumes<HGCalFlaggedECONDInfoCollection>(iConfig.getParameter<edm::InputTag>("FlaggedECONDInfo"))),
      metadataToken_(consumes<HGCalTestSystemMetaData>(iConfig.getParameter<edm::InputTag>("MetaData"))),
      moduleInfoToken_(
          esConsumes<HGCalCondSerializableModuleInfo, HGCalCondSerializableModuleInfoRcd, edm::Transition::BeginRun>()),
      siModuleInfoToken_(esConsumes<HGCalCondSerializableSiCellChannelInfo,
                         HGCalCondSerializableSiCellChannelInfoRcd,
                         edm::Transition::BeginRun>()),
      prescale_(std::max(1u, iConfig.getParameter<unsigned>("Prescale"))),
      min_num_evts_(iConfig.getParameter<unsigned>("MinimumNumEvents")) {}

//
HGCalDigisClient::~HGCalDigisClient() { LogDebug("HGCalDigisClient") << "End of the job" << std::endl; }

//
void HGCalDigisClient::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  bool toProcess = (num_processed_ < min_num_evts_) || (num_processed_ % prescale_ == 0);
  ++num_processed_;
  if (!toProcess)
    return;

  using namespace edm;

  //read digis and fill basic profiles
  const auto& digis = iEvent.getHandle(digisToken_);
  auto const& digis_view = digis->const_view();
  int32_t ndigis = digis_view.metadata().size();

  int cm_size = 12;
  int globalChannelId_cm[cm_size] = {37, 38, 76, 77, 115, 116, 154, 155, 193, 194, 232, 233};

  //start by computing the #fired TDC per e-Rx, average CM per e-Rx
  std::map<HGCalElectronicsId, float> nTDC, avgCM;
  for (int32_t i = 0; i < ndigis; ++i) {
    auto digi = digis_view[i];
    HGCalElectronicsId eleid = HGCalElectronicsId(digi.electronicsId());
    HGCalElectronicsId erxid(eleid.zSide(), eleid.fedId(), eleid.captureBlock(), eleid.econdIdx(), eleid.econdeRx(), 0);
    if (nTDC.count(erxid) == 0) {
      nTDC[erxid] = 0;
      avgCM[erxid] = 0;
    }
    if (eleid.isCM()) {
      avgCM[erxid] += 0.5 * digi.adc();
    } else if (digi.tctp() > 0 || digi.toa() > 0) {
      nTDC[erxid] += 1;
    }
    avgCM[erxid] = 0.5 * digi.cm();
  }

  //fill the histograms / statistics
  std::map<MonitorKey_t, std::pair<double, double> > maxVarADC;
  const auto& metadata = iEvent.get(metadataToken_);
  int trigTime = metadata.trigTime_;
  for (int32_t i = 0; i < ndigis; ++i) {
    auto digi = digis_view[i];

    //identify the module key from the electronics id
    HGCalElectronicsId eleid = HGCalElectronicsId(digi.electronicsId());
    HGCalElectronicsId erxid(eleid.zSide(), eleid.fedId(), eleid.captureBlock(), eleid.econdIdx(), eleid.econdeRx(), 0);
    bool isCM = eleid.isCM();
    int ch = eleid.halfrocChannel();
    int globalChannelId = 39 * eleid.econdeRx() + ch;
    HGCalSiCellChannelInfo channelInfo=eleidtoSiInfo_[eleid.raw()];
    MonitorKey_t logiKey(eleid.zSide(), eleid.fedId(), eleid.captureBlock(), eleid.econdIdx());
    if (module_keys_.count(logiKey) == 0) {
      continue;
    }
    MonitorKey_t geomKey = module_keys_[logiKey];
    LogDebug("HGCalDigisClient") << "Electronics Id= 0x" << std::hex << eleid.raw() << std::dec << std::endl
                                 << "adc = " << digi.adc() << ", "
                                 << "adcm1 = " << digi.adcm1() << ", "
                                 << "tot = " << digi.tot() << ", "
                                 << "halfrocChannel = " << (uint32_t)eleid.halfrocChannel() << ", "
                                 << "channel = " << globalChannelId << ", CM=" << isCM;

    // fill monitoring elements for digis
    //uint8_t tctp = digi.tctp();
    uint16_t adc = digi.adc();
    uint16_t tot = digi.tot();
    uint16_t toa = digi.toa();
    uint16_t adcm = digi.adcm1();

    if (adc > 0 || tot > 0)
      p_hitcount[geomKey]->Fill(globalChannelId);

    p_adcm[geomKey]->Fill(globalChannelId, adcm);
    h_adcm[geomKey]->Fill(adcm);
    p_toa[geomKey]->Fill(globalChannelId, toa);
    h_toa[geomKey]->Fill(toa);

    if (tot>0) {
      //TOT
      p_tot[geomKey]->Fill(globalChannelId, tot);
      h_tot[geomKey]->Fill(tot);
    } 

    if(adc>0) {

      //ADC
      h_adc[geomKey]->Fill(adc);
      p_adc[geomKey]->Fill(globalChannelId, adc);

      // Fill the p_adc plot for the common mode channels
      int* cm_begin = globalChannelId_cm;
      int* cm_end = cm_begin + cm_size;
      if (cm_end != std::find(cm_begin, cm_end, globalChannelId + 2)) {
        p_adc[geomKey]->Fill(globalChannelId + 2, avgCM[erxid]);
      }

      //after some events check which channel has the largest variance
      //it probably is where the beam is hitting, or the noisiest
      if (num_processed_ > 10) {
        if (maxVarADC.count(geomKey) == 0)
          maxVarADC[geomKey] = std::pair<double, double>(0.f, 0.f);
        double sumsq = p_sums[geomKey]->getBinContent(globalChannelId, 3);
        double sum = p_sums[geomKey]->getBinContent(globalChannelId, 2);
        double n = p_sums[geomKey]->getBinContent(globalChannelId, 1);
        double v = sumsq - pow(sum / n, 2);
        if (v > maxVarADC[geomKey].first) {
          maxVarADC[geomKey] = std::pair<double, double>(v, adc);
        }
      }

      //find the channel with the maximum adc-adcm
      //do not use unconnected(-1) or calib (0)
      if (num_processed_ > 400 && channelInfo.t==1) {  // ch < 36) {
        if (geomKey_maxADCpedsub.count(geomKey) == 0)
          geomKey_maxADCpedsub[geomKey] = std::pair<int, double>(-1, -1.);
        double sumADC = p_sums[geomKey]->getBinContent(globalChannelId, 2);
        double sumADCm = p_sums[geomKey]->getBinContent(globalChannelId, 7);
        double n = p_sums[geomKey]->getBinContent(globalChannelId, 1);
        double v = (sumADC - sumADCm) / n;
        if (v > geomKey_maxADCpedsub[geomKey].second) {
          geomKey_maxADCpedsub[geomKey].first = globalChannelId;
          geomKey_maxADCpedsub[geomKey].second = v;
        }
      }

      //fill histograms for the channel with maximum adc-adcm
      if (globalChannelId == geomKey_maxADCpedsub[geomKey].first) {
        double sumADCm = p_sums[geomKey]->getBinContent(globalChannelId, 7);
        double n = p_sums[geomKey]->getBinContent(globalChannelId, 1);
        double pedestal = sumADCm / n;
        if (tot > 0)
          p_totvstrigtime[geomKey]->Fill(trigTime, tot);
        if(adc>0) {
          p_adcvstrigtime[geomKey]->Fill(trigTime, adc - pedestal);
          p_adcpedsubvstrigtime[geomKey]->Fill(trigTime, adc);
        }
        if (toa > 0)
          p_toavstrigtime[geomKey]->Fill(trigTime, toa);
      }

      //increment sums of this channel
      float cm = avgCM[erxid];
      float tdc = nTDC[erxid];
      std::vector<double> tosum = {1,
                                   (double)adc,
                                   pow((double)adc, 2),
                                   (double)cm,
                                   pow((double)cm, 2),
                                   double(adc * cm),
                                   (double)adcm,
                                   pow((double)adcm, 2),
                                   double(adc * adcm),
                                   (double)tdc,
                                   pow((double)tdc, 2),
                                   double(adc * tdc)};
      for (size_t k = 1; k <= tosum.size(); k++)
        p_sums[geomKey]->setBinContent(
            globalChannelId, k, p_sums[geomKey]->getBinContent(globalChannelId, k) + tosum[k - 1]);
      
      //also add the TOA/TOT sums
      if(toa>0){
        p_sums[geomKey]->setBinContent( globalChannelId, 13, p_sums[geomKey]->getBinContent(globalChannelId, 13) + 1);
        p_sums[geomKey]->setBinContent( globalChannelId, 14, p_sums[geomKey]->getBinContent(globalChannelId, 14) + double(toa));
        p_sums[geomKey]->setBinContent( globalChannelId, 15, p_sums[geomKey]->getBinContent(globalChannelId, 15) + double(toa*toa));
      }
      if(tot>0){
        p_sums[geomKey]->setBinContent( globalChannelId, 16, p_sums[geomKey]->getBinContent(globalChannelId, 16) + 1);
        p_sums[geomKey]->setBinContent( globalChannelId, 17, p_sums[geomKey]->getBinContent(globalChannelId, 17) + double(tot));
        p_sums[geomKey]->setBinContent( globalChannelId, 18, p_sums[geomKey]->getBinContent(globalChannelId, 18) + double(tot*tot));
      }

      // For the last itetration, get the information from the CM and add it in the ADC row for the CM channels
      // so that the hex_pedestal maps will be filled for the CM channels in the HGCalDigisClientHarvester.cc
      if (i == ndigis - 1) {
        for (int icm = 0; icm < cm_size; icm++) {
          p_sums[geomKey]->setBinContent(
              globalChannelId_cm[icm], 1, p_sums[geomKey]->getBinContent(globalChannelId_cm[icm] - 2, 1));
          p_sums[geomKey]->setBinContent(
              globalChannelId_cm[icm], 2, p_sums[geomKey]->getBinContent(globalChannelId_cm[icm] - 2, 4));
          p_sums[geomKey]->setBinContent(
              globalChannelId_cm[icm], 3, p_sums[geomKey]->getBinContent(globalChannelId_cm[icm] - 2, 5));
        }
      }
    }
  }

  //read trigtime and fill monitoring elements for the leading hit
  LogDebug("HGCalDigisClient") << "trigTime=" << trigTime;
  for (auto it : maxVarADC) {
    double adc = it.second.second;
    p_maxadcvstrigtime[it.first]->Fill(trigTime, adc);
    p_maxadc[it.first]->Fill(adc);
  }

  //fill histogram for trigtime distribution
  h_trigtime->Fill(trigTime);

  //read flagged ECON-D list
  const auto& flagged_econds = iEvent.getHandle(econdQualityToken_);
  if (flagged_econds.isValid()) {
    for (auto econd : *flagged_econds) {
      HGCalElectronicsId eleid(econd.eleid);
      MonitorKey_t logiKey(eleid.zSide(), eleid.fedId(), eleid.captureBlock(), eleid.econdIdx());
      MonitorKey_t k = module_keys_[logiKey];
      int ibin = modbins_[k];
      p_econdpayload->Fill(ibin,econd.payload);
      p_econdcbquality->Fill(ibin,econd.cbflags);
      if (econd.cbFlag())
        p_econdquality->Fill(ibin, 0);
      if (econd.htFlag())
        p_econdquality->Fill(ibin, 1);
      if (econd.eboFlag())
        p_econdquality->Fill(ibin, 2);
      if (econd.matchFlag())
        p_econdquality->Fill(ibin, 3);
      if (econd.truncatedFlag())
        p_econdquality->Fill(ibin, 4);
      if (econd.wrongHeaderMarker())
        p_econdquality->Fill(ibin, 5);
      if (econd.payloadOverflows())
        p_econdquality->Fill(ibin, 6);
      if (econd.payloadMismatches())
        p_econdquality->Fill(ibin, 7);
    }
  }
}

//
void HGCalDigisClient::bookHistograms(DQMStore::IBooker& ibook, edm::Run const& run, edm::EventSetup const& iSetup) {
  //create module keys
  auto moduleInfo = iSetup.getData(moduleInfoToken_);
  module_keys_ = moduleInfo.getAsSimplifiedModuleLocatorMap(true);
  size_t nmods = module_keys_.size();
  LogDebug("HGCalDigisClient") << "Read module info with " << nmods << " entries";

  //map also the cell types
  const auto &siCellInfo = iSetup.getData(siModuleInfoToken_);
  eleidtoSiInfo_ = hgcal::mapEleIdToSiInfo(moduleInfo,siCellInfo); 
  
  //book monitoring elements (histos, profiles, etc.)
  ibook.setCurrentFolder("HGCAL/Digis");
  p_econdquality = ibook.book2D("p_econdquality", ";ECON-D;Header quality flags", nmods, 0, nmods, 8, 0, 8);
  p_econdquality->setBinLabel(1, "CB", 2);
  p_econdquality->setBinLabel(2, "H/T", 2);
  p_econdquality->setBinLabel(3, "E/B/O", 2);
  p_econdquality->setBinLabel(4, "M", 2);
  p_econdquality->setBinLabel(5, "Trunc", 2);
  p_econdquality->setBinLabel(6, "Marker", 2);
  p_econdquality->setBinLabel(7, "Payload (OF)", 2);
  p_econdquality->setBinLabel(8, "Payload (mismatch)", 2);

  p_econdcbquality = ibook.book2D("p_econdcbquality", ";ECON-D;DAQ quality flags", nmods, 0, nmods, 8, 0, 8);
  p_econdcbquality->setBinLabel(1, "Normal ", 2);
  p_econdcbquality->setBinLabel(2, "Payload", 2);
  p_econdcbquality->setBinLabel(3, "CRC Error", 2);
  p_econdcbquality->setBinLabel(4, "EvID Mis.", 2);
  p_econdcbquality->setBinLabel(5, "FSM T/O", 2);
  p_econdcbquality->setBinLabel(6, "BCID/OrbitID", 2);
  p_econdcbquality->setBinLabel(7, "MB Overflow", 2);
  p_econdcbquality->setBinLabel(8, "Innactive", 2);

  p_econdpayload= ibook.book2D("p_econdpayload", ";ECON-D;Payload", nmods, 0, nmods, 200, 0,500);

  h_trigtime = ibook.book1D("trigtime", ";trigger phase; Counts", 200, 0, 200);

  for (auto m : moduleInfo.params_) {

    TString tag = Form("zside%d_plane%d_u%d_v%d", m.zside, m.plane, m.u, m.v);
    MonitorKey_t k(m.zside, m.plane, m.u, m.v);
    modbins_[k] = modbins_.size();
    TString modlabel(tag);
    p_econdquality->setBinLabel(modbins_[k] + 1, modlabel.ReplaceAll("_", ",").Data(), 1);
    p_econdcbquality->setBinLabel(modbins_[k] + 1, modlabel.ReplaceAll("_", ",").Data(), 1);
    p_econdpayload->setBinLabel(modbins_[k] + 1, modlabel.ReplaceAll("_", ",").Data(), 1);

    int nch(39 * 6 * (1 + m.isHD));
    p_hitcount[k] = ibook.book1D("hitcount_" + tag, ";Channel; #hits", nch, 0, nch);
    p_maxadcvstrigtime[k] =
        ibook.book2D("maxadc_vs_trigtime_" + tag, ";trigger phase; max ADC of the event", 200, 0, 200, 100, 0, 1024);
    p_adcvstrigtime[k] = ibook.book2D(
        "adc_vs_trigtime_" + tag, ";trigger phase; ADC of channel with max <ADC-ADC_{-1}>", 200, 0, 200, 100, 0, 1024);
    p_adcpedsubvstrigtime[k] = ibook.book2D("adcpedsub_vs_trigtime_" + tag,
                                            ";trigger phase; ADC of channel with max <ADC-ADC_{-1}>",
                                            200,
                                            0,
                                            200,
                                            100,
                                            0,
                                            1024);
    p_totvstrigtime[k] = ibook.book2D(
        "tot_vs_trigtime_" + tag, ";trigger phase; TOT of channel with max <TOT>", 200, 0, 200, 100, 0, 4096);
    p_toavstrigtime[k] = ibook.book2D(
        "toa_vs_trigtime_" + tag, ";trigger phase; TOA of channel with max <ADC-ADC_{-1}>", 200, 0, 200, 100, 0, 1024);
    p_sums[k] = ibook.book2D("sums_" + tag, ";Channel;", nch, 0, nch, 18, 0, 18);
    p_sums[k]->setBinLabel(1, "N", 2);
    p_sums[k]->setBinLabel(2, "#sum ADC", 2);
    p_sums[k]->setBinLabel(3, "#sum ADC^{2}", 2);
    p_sums[k]->setBinLabel(4, "#sum CM", 2);
    p_sums[k]->setBinLabel(5, "#sum CM^{2}", 2);
    p_sums[k]->setBinLabel(6, "#sum ADC*CM", 2);
    p_sums[k]->setBinLabel(7, "#sum ADC_{-1}", 2);
    p_sums[k]->setBinLabel(8, "#sum ADC_{-1}^{2}", 2);
    p_sums[k]->setBinLabel(9, "#sum ADC*ADC_{-1}", 2);
    p_sums[k]->setBinLabel(10, "#sum N(TDC)", 2);
    p_sums[k]->setBinLabel(11, "#sum N(TDC)^{2}", 2);
    p_sums[k]->setBinLabel(12, "#sum ADC*N(TDC)", 2);
    p_sums[k]->setBinLabel(13, "N(TOA)", 2);
    p_sums[k]->setBinLabel(14, "#sum TOA", 2);
    p_sums[k]->setBinLabel(15, "#sum TOA^{2}", 2);
    p_sums[k]->setBinLabel(16, "N(TOT)", 2);
    p_sums[k]->setBinLabel(17, "#sum TOT", 2);
    p_sums[k]->setBinLabel(18, "#sum TOT^{2}", 2);

    p_maxadc[k] = ibook.book1D("maxadc_" + tag, ";max ADC; Counts", 100, 0, 1024);
    p_adc[k] = ibook.bookProfile("p_adc_" + tag, ";Channel; ADC", nch, 0, nch, 100, 0, 1024);
    p_adc[k]->setOption("s");  //save standard deviation instead of error mean for noise estimate
    p_tot[k] = ibook.bookProfile("p_tot_" + tag, ";Channel; TOT", nch, 0, nch, 100, 0, 1024);
    p_adcm[k] = ibook.bookProfile("p_adcm_" + tag, ";Channel; ADC(-1)", nch, 0, nch, 100, 0, 1024);
    p_toa[k] = ibook.bookProfile("p_toa_" + tag, ";Channel; TOA", nch, 0, nch, 100, 0, 1024);
    h_adc[k] = ibook.book1D("adc_" + tag, ";ADC; Counts (all channels)", 100, 0, 1024);
    h_adcpedsub[k] = ibook.book1D("adcpedsub_" + tag, ";ADC-ADC_{-1}; Counts (all channels)", 100, 0, 1024);
    h_adc[k] = ibook.book1D("adc_" + tag, ";ADC; Counts (all channels)", 100, 0, 1024);
    h_adcm[k] = ibook.book1D("adcm_" + tag, ";ADC_{-1}; Counts (all channels)", 100, 0, 1024);
    h_tot[k] = ibook.book1D("tot_" + tag, ";TOT; Counts (all channels)", 100, 0, 4096);
    h_toa[k] = ibook.book1D("toa_" + tag, ";TOA; Counts (all channels)", 100, 0, 1024);
  }
}

//
void HGCalDigisClient::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("Digis", edm::InputTag("hgcalDigis", ""));
  desc.add<edm::InputTag>("FlaggedECONDInfo", edm::InputTag("hgcalDigis", "UnpackerFlags"));
  desc.add<edm::InputTag>("MetaData", edm::InputTag("hgcalEmulatedSlinkRawData", "hgcalMetaData"));
  desc.add<unsigned>("Prescale", 1);
  desc.add<unsigned>("MinimumNumEvents", 10000);
  descriptions.addWithDefaultLabel(desc);
}

// define this as a plug-in
DEFINE_FWK_MODULE(HGCalDigisClient);
