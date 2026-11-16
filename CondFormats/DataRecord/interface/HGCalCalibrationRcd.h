#ifndef CondFormats_DataRecord_HGCalCalibrationRcd_h
#define CondFormats_DataRecord_HGCalCalibrationRcd_h

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/DataRecord/interface/HGCalADCPedestalsRcd.h"
#include "CondFormats/DataRecord/interface/HGCalNoiseRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCommonModePedestalsRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCommonModeSlopeRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCalibrationValidityRcd.h"
#include "FWCore/Framework/interface/DependentRecordImplementation.h"
#include "FWCore/Utilities/interface/mplVector.h"

/**
 * Record for the derived HGCalCalibParamHost calibration.
 * In SQLite mode its validity follows the mapping and the five conditions.
 * The JSON producer consumes only the electronics mapping.
 */
class HGCalCalibrationRcd
    : public edm::eventsetup::DependentRecordImplementation<
          HGCalCalibrationRcd,
          edm::mpl::Vector<HGCalElectronicsMappingRcd,
                           HGCalADCPedestalsRcd,
                           HGCalNoiseRcd,
                           HGCalCommonModePedestalsRcd,
                           HGCalCommonModeSlopeRcd,
                           HGCalCalibrationValidityRcd>> {};

#endif
