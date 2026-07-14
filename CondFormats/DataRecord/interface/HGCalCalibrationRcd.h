#ifndef CondFormats_DataRecord_HGCalCalibrationRcd_h
#define CondFormats_DataRecord_HGCalCalibrationRcd_h

#include "FWCore/Framework/interface/EventSetupRecordImplementation.h"

/**
 * EventSetup record carrying the persistent HGCal calibration payload.
 */
class HGCalCalibrationRcd
    : public edm::eventsetup::EventSetupRecordImplementation<HGCalCalibrationRcd> {};

#endif
