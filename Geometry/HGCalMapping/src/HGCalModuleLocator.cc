#include "Geometry/HGCalMapping/interface/HGCalModuleLocator.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/Utilities/interface/Exception.h"

//
void HGCalModuleLocator::buildModuleLocatorFrom(std::string path,bool usefip)
{
  if(usefip) {
    edm::FileInPath fip(path);
    path=fip.fullPath();
  }
  std::ifstream file(path);

  std::string line;
  size_t iline(0);
  while(std::getline(file, line))
  {
    iline++;
    if(iline==1) continue;
    std::istringstream stream(line);
    int zside;
    HGCalModuleInfo m;
    stream >> m.plane >> m.u >> m.v >> m.isSiPM >> m.isHD >> m.wafType >> m.econdidx >> m.captureblock >> m.slink >> m.captureblockidx >> m.fedid >> zside;
    m.zside = (zside>0);
    getInfo().addParameter(m);
  }
}
