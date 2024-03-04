/****************************************************************************
 *
 * This is a part of HGCal offline software.
 * Authors:
 *   Lovisa Rygaard
 *
 ****************************************************************************/

#ifndef Geometry_HGCalMapping_HGCalModuleLocator_H
#define Geometry_HGCalMapping_HGCalModuleLocator_H

#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalEntityLocatorBase.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

/**
   @short this class steers the parsing and the filling of the module info from txt files
 */
class HGCalModuleLocator : public HGCalEntityLocatorBase<HGCalCondSerializableModuleInfo> {

public:
  
  HGCalModuleLocator(){};
  
  void buildLocatorFrom(std::string path,bool usefip=false) override {
    buildModuleLocatorFrom(path,usefip);
  }
  
  void buildModuleLocatorFrom(std::string path,bool usefip=false);

  ~HGCalModuleLocator(){};
  
private:

};

#endif
