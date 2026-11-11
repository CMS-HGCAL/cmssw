#include "CondFormats/Serialization/interface/Test.h"
#include "CondFormats/HGCalObjects/src/headers.h"

int main() {
  // calibration conditions
  testSerialization<HGCalADCPedestalsModulePayload>();
  testSerialization<HGCalNoiseModulePayload>();
  testSerialization<HGCalCommonModePedestalsModulePayload>();
  testSerialization<HGCalCommonModeSlopeModulePayload>();
  testSerialization<HGCalCalibrationValidityModulePayload>();
  testSerialization<HGCalADCPedestalsPayload>();
  testSerialization<HGCalNoisePayload>();
  testSerialization<HGCalCommonModePedestalsPayload>();
  testSerialization<HGCalCommonModeSlopePayload>();
  testSerialization<HGCalCalibrationValidityPayload>();

  //dense indexers
  testSerialization<HGCalDenseIndexerBase>();
  testSerialization<HGCalMappingCellIndexer>();
  testSerialization<HGCalFEDReadoutSequence>();
  testSerialization<HGCalMappingModuleIndexer>();
  testSerialization<HGCalMappingCellIndexerTrigger>();
  testSerialization<HGCalTriggerFEDReadoutSequence>();
  testSerialization<HGCalMappingModuleIndexerTrigger>();

  return 0;
}
