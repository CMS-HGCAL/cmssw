#include "CondFormats/Serialization/interface/Test.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"

int main()
{
  //dense indexers  
  testSerialization<HGCalMappingCellIndexer>();
  testSerialization<HGCalMappingModuleIndexParameters>();
  testSerialization<HGCalMappingModuleIndexer>();

  return 0;
}
