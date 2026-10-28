#include <cstddef>
#include <sstream>

// CMSSW imports
#include "FWCore/MessageLogger/interface/MessageLogger.h"

// Alpaka imports
#include "HeterogeneousCore/AlpakaInterface/interface/traits.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

// HGCal imports
#include "SimCalorimetry/HGCalSimProducers/interface/alpaka/HGCalDigitizerAlgorithms.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  // @short subtract pedestals, linearize ADC & TOT to charge (fC), and convert to energy (GeV)
  struct HGCalAgeingKernel_scaleResponse {
    ALPAKA_FN_ACC void operator()(Acc1D const& acc,
                                  HGCalSimHitAccumDevice::View g4Hits,
                                  HGCalAgeingCondsDevice::ConstView conds) const {
      /*
      auto adc_denoise =
          [&](uint32_t adc, uint32_t cm, uint32_t adcm1, float adc_ped, float cm_slope, float cm_ped, float bxm1_slope) {
            float cmf = cm_slope * (0.5 * float(cm) - cm_ped);
            return ((adc - adc_ped) - cmf - bxm1_slope * (adcm1 - adc_ped - cmf));
          };

      auto tot_linearization =
          [&](uint32_t tot, float tot_lin, float tot2adc, float tot_ped, float tot_p0, float tot_p1, float tot_p2) {
            bool isLin(tot > tot_lin);
            bool isNotLin(!isLin);
            return isLin * (tot2adc * (tot - tot_ped)) + isNotLin * (tot_p0 + tot_p1 * tot + tot_p2 * tot * tot);
          };
      */

      for (auto idx : uniform_elements(acc, g4Hits.metadata().size())) {
        printf("%d\n",idx);

        /*
        auto calib = calibs[idx];
        bool calibvalid = calib.valid();
        auto digi = digis[idx];
        auto digiflags = digi.flags();
        bool isAvailable((digiflags != ::hgcal::DIGI_FLAG::Invalid) &&
                         (digiflags != ::hgcal::DIGI_FLAG::NotAvailable) && calibvalid);
        bool useTOT((digi.tctp() == 3) && isAvailable);
        bool useADC(!useTOT && isAvailable);
        auto lin_counts = useADC * adc_denoise(digi.adc(),
                                               digi.cm(),
                                               digi.adcm1(),
                                               calib.ADC_ped(),
                                               calib.CM_slope(),
                                               calib.CM_ped(),
                                               calib.BXm1_slope()) +
                          useTOT * tot_linearization(digi.tot(),
                                                     calib.TOT_lin(),
                                                     calib.TOTtoADC(),
                                                     calib.TOT_ped(),
                                                     calib.TOT_P0(),
                                                     calib.TOT_P1(),
                                                     calib.TOT_P2());

        // after denoising/linearization apply the MIP & EM scale to convert to energy (GeV)
        // noise is also saved in units of GeV
        recHits[idx].mipEnergy() = lin_counts * calib.MIPS_scale();
        recHits[idx].energy() = calib.EM_scale() * calib.MIPS_scale() * lin_counts;
        recHits[idx].sigmaNoise() = calib.EM_scale() * calib.MIPS_scale() * calib.Noise();
        */
      }
    }
  };


  // @short apply all calibration kernels
  HGCalSimHitAccumDevice HGCalAgeingAlgorithms::age(
      Queue& queue,
      HGCalSimHitAccumHost const& host_g4Hits, 
      HGCalAgeingCondsDevice const& ageConds) const {


    print_acc_host(host_g4Hits, 10);

    edm::LogWarning("HGCalAgeingAlgorithms") << "Start of ageing: copying accumulators to device" << std::endl;
    auto const ncells = host_g4Hits.view().metadata().size();
    HGCalSimHitAccumDevice device_g4Hits(queue, ncells);
    alpaka::memcpy(queue, device_g4Hits.buffer(), host_g4Hits.const_buffer());

    // number of items per group and division in groups to cover the whole problem
    uint32_t items = n_threads_;
    uint32_t groups = divide_up_by(ncells, items);

    // generate the working grid
    //   - threads with a single element per thread on a GPU backend
    //   - elements within a single thread on a CPU backend
    auto grid = make_workdiv<Acc1D>(groups, items);
    edm::LogWarning("HGCalAgeingAlgorithms") << "N groups: " << groups << "\tN items: " << items << std::endl;

    //execute ageing kernels
    alpaka::exec<Acc1D>(queue,
                        grid,
                        HGCalAgeingKernel_scaleResponse{},
                        device_g4Hits.view(),
                        ageConds.const_view());

    return device_g4Hits;
  }

  //
  void HGCalAgeingAlgorithms::print_acc_host(HGCalSimHitAccumHost const& g4Hits, int max) const {
   int max_ = max > 0 ? max : g4Hits.view().metadata().size();

    //lambdas for header and rows
    auto printHeader = [&](const char* label, const int n) {
      std::ostringstream os;
      os << "| " << label << " |";
      for (int k = 0; k < n; k++) os << " " << k << " |";
      edm::LogWarning("HGCalAgeingAlgorithms") << os.str();
    };
    auto printRow = [&](const char* label, const int n, auto values) {
      std::ostringstream os;
      os << "| " << label << " |";
      for (int k = 0; k < n; k++) {
        os << " " << values()(k, 0) << " |";
      }
      edm::LogWarning("HGCalAgeingAlgorithms") << os.str();
    };

    const auto ncols = hgcaldigi::kTimeBins;
    for (int i = 0; i < max_; i++) {
      auto const& hit = g4Hits.view()[i];
      edm::LogWarning("HGCalAgeingAlgorithms") << "Hit " << i;
      printHeader("Bucket", ncols);
      printRow("E",   ncols, [&]() -> decltype(auto) { return hit.sumE(); });
      printRow("E*T", ncols, [&]() -> decltype(auto) { return hit.sumExT(); });
    }
  }

  /* FIXME
  void HGCalAgeingAlgorithms::print_recHit_device(Queue& queue,
                                                             HGCalSoARecHitsHostCollection::View const& recHits,
                                                             int max) const {
    auto grid = make_workdiv<Acc1D>(1, 1);
    auto size = max > 0 ? max : recHits.metadata().size();
    alpaka::exec<Acc1D>(queue, grid, HGCalAgeingKernel_printRecHits{}, recHits, size);

    // ensure that the print operations are complete before returning
    alpaka::wait(queue);
  }

  */

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE
