#ifndef CondFormats_DataRecord_HGCalCalibrationRcd_h
#define CondFormats_DataRecord_HGCalCalibrationRcd_h

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "FWCore/Framework/interface/DependentRecordImplementation.h"
#include "FWCore/Utilities/interface/mplVector.h"

/**
 * EventSetup record carrying the persistent HGCal calibration payload.
 *
 * It depends on the electronics mapping record because the typed
 * calibration payload must be expanded into the channel-indexed
 * HGCalCalibParamHost representation.
 */
class HGCalCalibrationRcd
    : public edm::eventsetup::DependentRecordImplementation<
          HGCalCalibrationRcd,
          edm::mpl::Vector<HGCalElectronicsMappingRcd>> {};

#endif
