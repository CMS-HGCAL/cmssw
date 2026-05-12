#ifndef DataFormats_HGCalDigi_interface_HGCalDigiSoA_h
#define DataFormats_HGCalDigi_interface_HGCalDigiSoA_h

#include <Eigen/Core>

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"

namespace hgcaldigi {

  // Generate structure of arrays (SoA) layout with Digi dataformat
  GENERATE_SOA_LAYOUT(HGCalDigiSoALayout,
                      SOA_EIGEN_COLUMN(Eigen::Matrix<uint16_t, 12, 1>, cmsum),
                      SOA_COLUMN(uint16_t, msubchidx),
                      SOA_COLUMN(uint16_t, msuberxidx),
                      SOA_COLUMN(uint16_t, cellfrac),
                      SOA_COLUMN(uint16_t, msubunconnectedch),
                      SOA_COLUMN(uint16_t, ntoa),
                      SOA_COLUMN(uint16_t, ntot))
  using HGCalDigiSoA = HGCalDigiSoALayout<>;

}  // namespace hgcaldigi

#endif  // DataFormats_HGCalDigi_interface_HGCalDigiSoA_h
