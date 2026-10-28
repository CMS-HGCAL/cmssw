#ifndef SimCalorimetry_HGCalSimProducers_interface_alpaka_HGCalDigitizerAlgorithms_h
#define SimCalorimetry_HGCalSimProducers_interface_alpaka_HGCalDigitizerAlgorithms_h

// Alpaka imports
#include <alpaka/alpaka.hpp>
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

// Host & device HGCal formats to handle
#include "DataFormats/HGCalDigi/interface/HGCalSimHitAccumHost.h"
#include "DataFormats/HGCalDigi/interface/alpaka/HGCalSimHitAccumDevice.h"
#include "CondFormats/HGCalObjects/interface/HGCalAgeingCondsHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalAgeingCondsDevice.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace hgcaldigi;
  using namespace hgcal;

  class HGCalAgeingAlgorithms {
  public:
    HGCalAgeingAlgorithms(int n_blocks, int n_threads) : n_blocks_(n_blocks), n_threads_(n_threads) {}

    /**
      @short steer the applicaiton of ageing and other realistic reponse algorithms
    */
    HGCalSimHitAccumDevice age(Queue&, HGCalSimHitAccumHost const&, HGCalAgeingCondsDevice const&) const;

  private:
    /**
      @short debugging printouts
    */
    void print_acc_host(HGCalSimHitAccumHost const&, int max = -1) const;

    //FIXME
    //void print_recHit_device(Queue& queue, HGCalSoARecHitsHostCollection::View const& recHits, int max = -1) const;

    int n_blocks_, n_threads_;
  };

  /*
  FIXME
  class HGCalROCDigitizerAlgorithms {
  public:
    HGCalROCDigitizerAlgorithms(int n_blocks, int n_threads) : n_blocks_(n_blocks), n_threads_(n_threads) {}

    HGCalROCDigisAccumDevice digitize(Queue& queue, HGCalSimHitAccumHost const&, HGCalAgeingCondsDevice const&) const;

  private:
    //void print(HGCalDigiHost const& digis, int max = -1) const;
    //void print_digi_device(HGCalDigiDevice const& digis, int max = -1) const;
    //void print_recHit_device(Queue& queue, HGCalSoARecHitsHostCollection::View const& recHits, int max = -1) const;

    int n_blocks_;
    int n_threads_;
  };

*/

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

#endif  // SimCalorimetry_HGCalSimProducers_interface_alpaka_HGCalDigitizerAlgorithms_h
