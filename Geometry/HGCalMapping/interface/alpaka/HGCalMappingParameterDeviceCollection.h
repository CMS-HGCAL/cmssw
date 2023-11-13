#ifndef Geometry_HGCalMapping_interface_alpaka_HGCalMappingParameterHostCollection_h
#define Geometry_HGCalMapping_interface_alpaka_HGCalMappingParameterHostCollection_h

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingParameterSoA.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingParameterHostCollection.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcal {

    using namespace ::hgcal;
    using HGCalMappingModuleParamDeviceCollection = PortableCollection<HGCalMappingModuleParamSoA>;
    using HGCalMappingCellParamDeviceCollection = PortableCollection<HGCalMappingCellParamSoA>;

  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

#endif  // Geometry_HGCalMapping_interface_alpaka_HGCalMappingParameterHostCollection_h