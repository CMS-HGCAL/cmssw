#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableSiPMTileInfo.h"
#include "FWCore/Utilities/interface/EDMException.h"

// #include <iostream>
// #include <fstream>
// #include <sstream>
// #include <tuple>
#include <iterator>
#include <algorithm>

//
std::vector<HGCalSiPMTileInfo> HGCalCondSerializableSiPMTileInfo::getAllCellsInModule(int layer, int modiring) const {
  
  std::vector<HGCalSiPMTileInfo> modules;
  std::copy_if(params_.begin(), params_.end(), std::back_inserter(modules), [&](HGCalSiPMTileInfo v) {
     return (v.plane == layer) && (v.modiring == modiring);
  });
  
  return modules;
}


int HGCalCondSerializableSiPMTileInfo::getSiPMchannel(uint8_t econderx, uint8_t halfrocch) {
  
  return halfrocch + 36*econderx;
}


std::tuple<int,int,int> HGCalCondSerializableSiPMTileInfo::getCellLocation(int econderx, int halfrocch, int layer, int modiring) {
  
  int sipmcell = getSiPMchannel(econderx, halfrocch);

  std::cout << sipmcell << ", " << econderx << ", " << halfrocch << ", " << layer << ", " << modiring << std::endl;

  auto _matchesByChannel = [sipmcell, layer, modiring](HGCalSiPMTileInfo c) {
    return c.sipmcell == sipmcell && c.plane == layer && c.modiring == modiring;
  };
  auto it = std::find_if(begin(params_), end(params_), _matchesByChannel);
  if(it==params_.end()) {
    edm::Exception e(edm::errors::NotFound,"HGCalCondSerializableSiPMTileInfo::getCellLocation: Failed to match cell by channel number");
    throw e;
  }
  return std::make_tuple(it->plane, it->iring, it->iphi);
}


std::tuple<int,int,int> HGCalCondSerializableSiPMTileInfo::getCellLocation(HGCalElectronicsId& id, int layer, int modiring) {

  return getCellLocation(id.econdeRx(),id.halfrocChannel(),layer,modiring);
}


DetId HGCalCondSerializableSiPMTileInfo::getDetId(HGCalElectronicsId& id, int z, int layer, int modiring, int modiphi) {
  
  int plane,ciring,ciphi;
  std::cout << layer << "  " << modiphi << std::endl;
  std::tie(plane,ciring,ciphi) =  getCellLocation(id.econdeRx(),id.halfrocChannel(),layer,modiring);

  // Layer desription has an offset of 25
  int idlayer = layer - 25;
  int idtype = ((idlayer <= 8) ? 0 : ((idlayer <= 17) ? 1 : 2));
  int ring = (z ? ciring : (-1)*ciring);
  // iphi currently calculated for SiPM modules with iphi 0-7 only, DetId iphi defined for 1-288
  int iphi = modiphi*8 + ciphi + 1;

  HGCScintillatorDetId detid(idtype, idlayer, ring, iphi, false, true);
  return detid;
}


std::tuple<int,int,int,bool> HGCalCondSerializableSiPMTileInfo::getModuleLocation(DetId& id) {
  DetId::Detector subdet = id.det();

  if (subdet == DetId::Detector::HGCalHSc) {
    HGCScintillatorDetId detid(id); 
    
    // Layer desription has an offset of 25
    int layer = detid.layer() + 25;
    int cellring = detid.ring();
    int modiring;
    if (layer <= 37) {
      modiring = ((cellring <= 25) ? 0 : 1);
    } else if (layer <= 43) {
      modiring = ((cellring <= 17) ? 0 : ((cellring <= 25 ) ? 1 : ((cellring <= 33) ? 2 : 3))); 
    } else {
      modiring = ((cellring <= 5) ? 0 : ((cellring <= 17 ) ? 1 : ((cellring <= 25) ? 2 : ((cellring <= 33) ? 3 : 4))));
    }
    // iphi currently calculated for SiPM modules with iphi 0-7 only, DetId iphi defined for 1-288
    int modiphi = (detid.iphi()-1)/8;
    bool zside = (detid.zside()>0);

    return std::make_tuple(layer,modiring,modiphi,zside);

  } else {
    throw cms::Exception("InvalidDetId") << "Wrong HGCal DetId::Detector in HGCalCondSerializableSiPMTileInfo::getModuleLocation.";
  }
}

std::tuple<uint8_t,uint8_t> HGCalCondSerializableSiPMTileInfo::getROCFields(DetId& id) {
  
  int layer,modiring,modiphi;
  bool zside;
  std::tie(layer,modiring,modiphi,zside) = getModuleLocation(id);
  
  DetId::Detector subdet = id.det();

  if (subdet == DetId::Detector::HGCalHSc) {
    HGCScintillatorDetId detid(id); 

    int iring = detid.ring();
    int iphi = detid.iphi() - modiphi*8 - 1;

    auto _matchesByGeom = [layer, iring, iphi, modiring](HGCalSiPMTileInfo c) {
    return c.plane == layer && c.iring == iring && c.iphi == iphi && c.modiring == modiring;
    };
    auto it = std::find_if(begin(params_), end(params_), _matchesByGeom);
    if(it==params_.end()) {
      edm::Exception e(edm::errors::NotFound,"HGCalCondSerializableSiPMTileInfo::getROCFields: Failed to match cell by gemoetry number");
      throw e;
    }
    uint8_t econderx = it->sipmcell/36;
    uint8_t halfrocch = it->sipmcell - 36*econderx;
    return std::make_tuple(econderx, halfrocch);

  } else {
    throw cms::Exception("InvalidDetId") << "Wrong HGCal DetId::Detector in HGCalCondSerializableSiPMTileInfo::getModuleLocation.";
  }
}