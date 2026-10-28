#ifndef DataFormats_HGCalDigi_interface_HGCalSimHitAccumHost_h
#define DataFormats_HGCalDigi_interface_HGCalSimHitAccumHost_h

#include "DataFormats/Portable/interface/PortableHostCollection.h"
#include "DataFormats/HGCalDigi/interface/HGCalSimHitAccumSoA.h"

namespace hgcaldigi {

  // SoA with Geant 4 hit sums in host memory
  using HGCalSimHitAccumHost = PortableHostCollection<HGCalSimHitAccumSoA>;

  // SoA with ROC 32b words in host memory
  using HGCalROCDigisAccumHost = PortableHostCollection<HGCalROCDigisAccumSoA>;

}  // namespace hgcaldigi

#endif  // DataFormats_HGCalDigi_interface_HGCalSimHitAccumHost_h
