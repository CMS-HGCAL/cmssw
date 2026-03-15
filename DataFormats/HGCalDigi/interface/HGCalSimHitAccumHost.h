#ifndef DataFormats_HGCalDigi_interface_HGCalSimHitAccumHost_h
#define DataFormats_HGCalDigi_interface_HGCalSimHitAccumHost_h

#include "DataFormats/Portable/interface/PortableHostCollection.h"
#include "DataFormats/HGCalDigi/interface/HGCalSimHitAccumSoA.h"

namespace hgcaldigi {

  // SoA with x, y, z, id fields in host memory
  using HGCalSimHitAccumHost = PortableHostCollection<HGCalSimHitAccumSoA>;

}  // namespace hgcaldigi

#endif  // DataFormats_HGCalDigi_interface_HGCalSimHitAccumHost_h
