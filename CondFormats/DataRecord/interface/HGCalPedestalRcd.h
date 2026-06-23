// HGCalPedestalRcd.h
//
// EventSetup Record for HGCal pedestal calibration constants.
// Associates HGCalPedestalConditions payload with CondDB.
//
// Usage in ESProducer:
//   #include "CondFormats/DataRecord/interface/HGCalPedestalRcd.h"
//   iRecord.get(pedestalToken_)
//
// Author: Fatma Nur Yuce (EP-UCM, RDH), CERN 2026

#ifndef CondFormats_DataRecord_HGCalPedestalRcd_h
#define CondFormats_DataRecord_HGCalPedestalRcd_h

#include "FWCore/Framework/interface/EventSetupRecordImplementation.h"

class HGCalPedestalRcd
    : public edm::eventsetup::EventSetupRecordImplementation<HGCalPedestalRcd> {};

#endif // CondFormats_DataRecord_HGCalPedestalRcd_h
