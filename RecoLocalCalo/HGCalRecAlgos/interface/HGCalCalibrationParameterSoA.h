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
                      SOA_SCALAR(HGCalCalibrationParameterIndex, config),
                      SOA_COLUMN(float, pedestal),
                      SOA_COLUMN(float, CM_slope),
                      SOA_COLUMN(float, CM_offset),
                      SOA_COLUMN(float, BXm1_slope),
                      SOA_COLUMN(float, BXm1_offset)
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
