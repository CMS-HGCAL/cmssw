#ifndef DataFormats_PortableTestObjects_interface_alpaka_HGCalSoARecHitsDeviceCollection_h
#define DataFormats_PortableTestObjects_interface_alpaka_HGCalSoARecHitsDeviceCollection_h

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"
#include "DataFormats/HGCalReco/interface/HGCalSoACMML.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {
  // SoA with 21 fields required for ML calibration.
  using HGCalSoACMMLDeviceCollection = PortableCollection<HGCalSoARecHits>;
}  
#endif
