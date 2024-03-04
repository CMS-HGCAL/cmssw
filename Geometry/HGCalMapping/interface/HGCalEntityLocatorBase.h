#ifndef Geometry_HGCalMapping_HGCalEntityLocatorBase_H
#define Geometry_HGCalMapping_HGCalEntityLocatorBase_H

#include "FWCore/Utilities/interface/Exception.h"
#include <string>

/**
   @short templated class for the parsing/filling of locator information
 */
template<class T>
class HGCalEntityLocatorBase {

  public:

    HGCalEntityLocatorBase(){}

    virtual void buildLocatorFrom(std::string path,bool usefip=false) {
      throw cms::Exception("HGCalEntityLocatorBase") << "buildLocatorFrom method needs to be overriden";
    }
    
    //returns the info
    T &getInfo() { return info_; }
    
    virtual ~HGCalEntityLocatorBase() {}
    
  private:
  
    //object holding all module info
    T info_;
};



#endif
