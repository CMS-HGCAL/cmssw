#ifndef DataFormats_HGCalDigi_interface_HGCalSimHitAccumSoA_h
#define DataFormats_HGCalDigi_interface_HGCalSimHitAccumSoA_h

#include <Eigen/Core>
#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"


namespace hgcaldigi {

  constexpr int kTimeBins = 12;
  using TimedAccumulatorF = Eigen ::Matrix<float, kTimeBins, 1>; 
  
  GENERATE_SOA_LAYOUT(HGCalSimHitAccumSoALayout,
      SOA_COLUMN(TimedAccumulatorF, sumE),
      SOA_COLUMN(TimedAccumulatorF, sumExT)
  )

  using HGCalSimHitAccumSoA = HGCalSimHitAccumSoALayout<>;

  struct G4HitLite {
    uint32_t idx;
    float energy;
    float time;
    int tbin;
  };
  
}

#endif
