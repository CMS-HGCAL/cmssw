#include "Geometry/HGCalMapping/interface/HGCalSiPMCellLocator.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

void HGCalSiPMCellLocator::buildLocatorFrom(std::string path, bool append, bool usefip)
{

  if(!append) getInfo().params_.clear();

  if(usefip){
    edm::FileInPath fip(path);
    path=fip.fullPath();
  }

  std::ifstream file(path);
  std::string line;
  size_t iline(0);
  if (file.is_open())
  {
    while (std::getline(file, line))
    {
      iline+=1;
      if(iline==1) continue;
      std::istringstream stream(line);

      HGCalSiPMTileInfo c; 
      stream >> c.sipmcell >> c.plane >> c.iring >> c.iphi >> c.type >> c.trigch >> c.trigsum >> c.modiring >> c.t;
      getInfo().addParameter(c);
    }
  }
  else
  {
    edm::Exception e(edm::errors::FileOpenError, "HGCalSiPMCellLocator::buildLocatorFrom : SiPM channel mapping file can not be found.");
    throw e;
  }
  file.close();

}
