#ifndef HGCalCondSerializableConfigRcd_HGCalCondSerializableConfigRcd_h
#define HGCalCondSerializableConfigRcd_HGCalCondSerializableConfigRcd_h
// -*- C++ -*-
//
// Package:     CondFormats/DataRecord
// Class  :     HGCalCondSerializableConfigRcd
//
/**\class HGCalCondSerializableConfigRcd HGCalCondSerializableConfigRcd.h CondFormats/DataRecord/interface/HGCalCondSerializableConfigRcd.h
 *
 * Description:
 *   Record for storing HGCalCondSerializableConfig from CondFormats/HGCalObjects/interface/HGCalCondSerializableConfig.h
 *   This record is used for passing the configuration parameters to the calibration step in RAW -> RECO,
 *   This record depends on the module info.
 *
 */
//
// Author:      Pedro Da Silva, Izaak Neutelings
// Created:     Mon, 29 May 2023 09:13:07 GMT
//

#include "FWCore/Framework/interface/DependentRecordImplementation.h"
#include "FWCore/Utilities/interface/mplVector.h"
#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"

class HGCalCondSerializableConfigRcd
  : public edm::eventsetup::DependentRecordImplementation<HGCalCondSerializableConfigRcd,
      edm::mpl::Vector<HGCalCondSerializableModuleInfoRcd> > {};

#endif
