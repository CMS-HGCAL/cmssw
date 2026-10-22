#include "CondFormats/Serialization/interface/Test.h"
#include "CondFormats/HGCalObjects/src/headers.h"

int main() {
  // calibration conditions
  testSerialization<HGCalCalibrationModulePayload>();
  testSerialization<HGCalCalibrationPayload>();

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
