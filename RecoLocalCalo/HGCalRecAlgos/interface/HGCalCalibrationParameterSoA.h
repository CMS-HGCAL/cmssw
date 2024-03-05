#ifndef RecoLocalCalo_HGCalRecAlgos_interface_HGCalCalibrationParameterSoA_h
#define RecoLocalCalo_HGCalRecAlgos_interface_HGCalCalibrationParameterSoA_h

#include <Eigen/Core>
#include <Eigen/Dense>

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"
#include "DataFormats/SoATemplate/interface/SoAView.h"

#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalCalibrationParameterIndex.h"

namespace hgcalrechit {

  // Generate structure of channel-level arrays (SoA) layout with RecHit dataformat
  GENERATE_SOA_LAYOUT(HGCalCalibParamSoALayout,
                      SOA_SCALAR(HGCalCalibrationParameterIndex, config), // map elecId <-> dense idx
                      SOA_COLUMN(float, ADC_ped),     // ADC pedestals
                      SOA_COLUMN(float, CM_slope),    // common mode slope
                      SOA_COLUMN(float, CM_ped),      // common mode pedestal (offset)
                      SOA_COLUMN(float, BXm1_slope),  // leakage correction from previous bunch
                      SOA_COLUMN(float, ADCtofC),     // ADC conversion to charge (fC)
                      SOA_COLUMN(float, TOTtofC),     // TOT conversion to charge (fC)
                      SOA_COLUMN(float, TOT_ped),     // TOT pedestal (offset)
                      SOA_COLUMN(float, TOT_lin),     // threshold at which TOT is linear
                      SOA_COLUMN(float, TOT_P0),      // coefficient pol2 in nonlinear region
                      SOA_COLUMN(float, TOT_P1),      // coefficient pol2 in nonlinear region
                      SOA_COLUMN(float, TOT_P2),      // coefficient pol2 in nonlinear region
                      SOA_COLUMN(float, TOAtops),     // TOA conversion to time (ps)
                      SOA_COLUMN(bool,  valid)        // if false: mask dead channel
  )
  using HGCalCalibParamSoA = HGCalCalibParamSoALayout<>;

  //// Generate structure of channel-level arrays (SoA) layout with RecHit dataformat
  //GENERATE_SOA_LAYOUT(HGCalChannelConfigParamSoALayout,
  //                    SOA_SCALAR(HGCalCalibrationParameterIndex, config),
  //                    SOA_COLUMN(uint8_t, gain)
  //)
  //using HGCalChannelConfigParamSoA = HGCalChannelConfigParamSoALayout<>;

  // Generate structure of ROC-level arrays (SoA) layout with RecHit dataformat
  GENERATE_SOA_LAYOUT(HGCalConfigParamSoALayout,
                      SOA_SCALAR(HGCalCalibrationParameterIndex, config),
                      SOA_COLUMN(uint8_t, gain)
  )
  using HGCalConfigParamSoA = HGCalConfigParamSoALayout<>;

}  // namespace hgcalrechit

#endif  // RecoLocalCalo_HGCalRecAlgos_interface_HGCalCalibrationParameterSoA_h
