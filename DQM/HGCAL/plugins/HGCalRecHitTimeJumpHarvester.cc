#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "DQMServices/Core/interface/DQMEDHarvester.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "CondFormats/RunInfo/interface/LHCInfoPerFill.h"
#include "CondFormats/DataRecord/interface/LHCInfoPerFillRcd.h"
#include "TOffsetFitter.h"
#include "TFile.h"
#include "TFitResult.h"
#include "TFitResultPtr.h"
#include "TGraphErrors.h"
#include "TProfile.h"

class HGCalRecHitTimeHarvester : public DQMEDHarvester {
public:
  static constexpr auto NCRATES = 6;
  static constexpr auto MINLAYER = 25;
  static constexpr auto MAXLAYER = 27;
  static constexpr auto NLAYERS = MAXLAYER - MINLAYER;
  static constexpr auto MAXBX = LHCInfoPerFill::bunchSlots;

  explicit HGCalRecHitTimeHarvester(const edm::ParameterSet& config)
      : DQMEDHarvester(config),
        inputFile_(config.getParameter<std::string>("inputFile")),
        lhcInfoToken_(esConsumes<edm::Transition::BeginRun>(config.getParameter<edm::ESInputTag>("lhcInfo"))) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    DQMEDHarvester::fillDescription(desc);
    desc.add<std::string>("inputFile", "");
    desc.add<edm::ESInputTag>("lhcInfo", edm::ESInputTag("lhcPhaseShiftESSource", "lhcPhaseShift"));
    descriptions.add("hgcalRecHitTimeHarvester", desc);
  }

  void beginRun(const edm::Run& run, const edm::EventSetup& iSetup) override {
    const auto& lhcInfo = iSetup.getData(lhcInfoToken_);
    if (lhcInfo.injectionScheme() != lhcScheme_) {
      lhcScheme_ = lhcInfo.injectionScheme();
      // get train start edges
      const auto collidingBunchBitset = lhcInfo.bunchBitsetForBeam1() & lhcInfo.bunchBitsetForBeam2();
      std::vector<int> trainStartEdges;
      for (size_t i = 0; i < MAXBX; i++)
        if ((i == 0 || collidingBunchBitset[i - 1] == 0) && collidingBunchBitset[i] == 1)
          trainStartEdges.push_back(i);
      // fit phase shifts
      TGraph graph;
      for (size_t i = 0, j = 0; i < MAXBX; i++)
        if (collidingBunchBitset[i])
          graph.SetPoint(j++, i, lhcInfo.phaseShift()[i] * 1000.);  // units: [ns] -> [ps]
      // initialize model
      TOffsetFitter helper(NCRATES);
      helper.setTrainStartEdges(trainStartEdges);
      model_ = std::make_unique<TF1>("timeModel", helper, 0., MAXBX * NLAYERS * NCRATES, helper.npars());
      for (size_t i = 0; i < NCRATES + 2; i++)
        model_->FixParameter(i, 0);
      auto result = graph.Fit(model_.get(), "SNQ");
      for (size_t i = 0; i < NCRATES + 2; i++)
        model_->ReleaseParameter(i);
      for (size_t i = NCRATES + 2; i < helper.npars(); i++)
        model_->FixParameter(i, model_->GetParameter(i));
    }
  }

private:
  const std::string inputFile_;
  const edm::ESGetToken<LHCInfoPerFill, LHCInfoPerFillRcd> lhcInfoToken_;
  std::unique_ptr<TF1> model_;
  std::string lhcScheme_;

  void dqmEndJob(DQMStore::IBooker& booker, DQMStore::IGetter& getter) override {
    std::unique_ptr<TFile> source;
    if (!inputFile_.empty()) {
      source.reset(TFile::Open(inputFile_.c_str(), "READ"));
      if (!source || source->IsZombie())
        throw std::runtime_error("Cannot open DQM input file: " + inputFile_);
    }

    // extract values
    int point(0);
    TGraphErrors graph;
    for (const std::string endcap : {"Plus"}) {
      for (int layer = MINLAYER; layer < MAXLAYER; layer++) {
        const std::string folder = "EndCap_" + endcap + "/Layer_" + std::to_string(layer);
        const auto path = "DQMData/Run 1/HGCAL/Run summary/" + folder + "/rechittimevsbxcrateProfile";
        TProfile* profile(nullptr);
        source->GetObject(path.c_str(), profile);
        if (!profile || !std::string(profile->GetErrorOption()).empty()) {
          edm::LogWarning("HGCalRecHitTimeHarvester") << "Expected a TProfile with errors on the mean in " << folder;
          continue;
        }
        for (int bin = 1; bin <= profile->GetNbinsX(); ++bin) {
          if (profile->GetBinEntries(bin) < 10)
            continue;
          const auto mean = profile->GetBinContent(bin);
          const auto error = profile->GetBinError(bin);
          if (!std::isfinite(mean) || !std::isfinite(error) || error <= 0.)
            continue;
          const int xbin = profile->GetBinCenter(bin);
          const auto bunch = xbin % MAXBX;
          const auto crate = xbin / MAXBX - 1;
          const auto ilayer = layer - MINLAYER;
          const auto index = (ilayer * NCRATES + crate) * MAXBX + bunch;
          graph.SetPoint(point, index, mean);
          graph.SetPointError(point, 0., error);
          point += 1;
        }
      }
    }

    // perform fit
    if (point < model_->GetNpar()) {
      edm::LogWarning("HGCalRecHitTimeHarvester") << "Not enough points to perform fit!";
      return;
    }
    auto result = graph.Fit(model_.get(), "SN");
    if (int(result) != 0 || !result.Get() || !result->IsValid() || result->CovMatrixStatus() != 3) {
      edm::LogWarning("HGCalRecHitTimeHarvester") << "Crate fit failed!";
      return;
    }
  }
};

DEFINE_FWK_MODULE(HGCalRecHitTimeHarvester);
