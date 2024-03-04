#ifndef DataFormats_HGCalRecHit_interface_HGCalRecHitHostCollection_h
#define DataFormats_HGCalRecHit_interface_HGCalRecHitHostCollection_h

#include "DataFormats/Portable/interface/PortableHostCollection.h"
#include "DataFormats/HGCalRecHit/interface/HGCalRecHitSoA.h"

namespace hgcalrechit {

  // SoA with x, y, z, id fields in host memory
  using HGCalRecHitHostCollection = PortableHostCollection<HGCalRecHitSoA>;

}  // namespace hgcalrechit

#endif  // DataFormats_HGCalRecHit_interface_HGCalRecHitHostCollection_h
