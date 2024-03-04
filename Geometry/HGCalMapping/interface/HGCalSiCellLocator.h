#ifndef _geometry_hgcalmapping_hgcalsicelllocator_h_
#define _geometry_hgcalmapping_hgcalsicelllocator_h_

#include <string>
#include <vector>
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiCellChannelInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalEntityLocatorBase.h"

class HGCalSiCellLocator : public HGCalEntityLocatorBase<HGCalCondSerializableSiCellChannelInfo>
{

public:

  HGCalSiCellLocator(){};

  void buildLocatorFrom(std::string path,bool usefip=false) override {
    buildLocatorFrom(path,false,usefip);
  }

  void buildLocatorFrom(std::string url,bool append,bool usefip);  

  HGCalSiCellChannelInfo locateCellByGeom(int iu,int iv,uint8_t wafType, bool isHD);

  ~HGCalSiCellLocator(){};

private:

};



#endif
