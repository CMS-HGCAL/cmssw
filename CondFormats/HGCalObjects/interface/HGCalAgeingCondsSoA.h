#ifndef CondFormats_HGCalObjects_interface_HGCalAgeingCondsSoA_h
#define CondFormats_HGCalObjects_interface_HGCalAgeingCondsSoA_h

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"

namespace hgcal {

  // Generate structure of module-level (ECON-D) arrays (SoA) layout with module mapping information
  GENERATE_SOA_LAYOUT(HGCalAgeingCondsSoALayout,
                      SOA_COLUMN(float, fluence),
                      SOA_COLUMN(float, dose)
                      )
  using HGCalAgeingCondsSoA = HGCalAgeingCondsSoALayout<>;

}  // namespace hgcal

#endif  // CondFormats_HGCalObjects_interface_HGCalAgeingCondsSoA_h
