/****************************************************************************
 *
 * This is a part of HGCal offline software.
 * Authors:
 *   Lovisa Rygaard
 *
 ****************************************************************************/

#ifndef Geometry_HGCalMapping_HGCalSiPMCellLocator_H
#define Geometry_HGCalMapping_HGCalSiPMCellLocator_H

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiPMTileInfo.h"
#include "Geometry/HGCalMapping/interface/HGCalEntityLocatorBase.h"

class HGCalSiPMCellLocator : public HGCalEntityLocatorBase<HGCalCondSerializableSiPMTileInfo> 
{

    public:

        HGCalSiPMCellLocator(){};

        void buildLocatorFrom(std::string path,bool usefip=false) override {
            buildLocatorFrom(path,false,usefip);
        }

        void buildLocatorFrom(std::string path,bool append,bool usefip);

        ~HGCalSiPMCellLocator(){};

    private:

};

#endif
