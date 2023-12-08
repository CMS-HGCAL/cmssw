#include "Geometry/HGCalMapping/interface/HGCalMappingTools.h"

namespace hgcal {

  namespace mappingtools {

    uint16_t getEcondErx(uint16_t chip, uint16_t half){
      return chip*2+half;
    }

    uint32_t getElectronicsId(bool zside, uint16_t fedid, uint16_t captureblock, uint16_t econdidx, int cellchip, int cellhalf, int cellseq) {
      uint16_t econderx = getEcondErx(cellchip, cellhalf);

      return HGCalElectronicsId(zside,fedid,captureblock,econdidx,econderx,cellseq).raw();
    }

    uint32_t getSiDetId(bool zside, int moduleplane, int moduleu, int modulev, int modulethickness, int celliu, int celliv) {
    
      DetId::Detector det = moduleplane<=26 ? DetId::Detector::HGCalEE : DetId::Detector::HGCalHSi;
      int zp(zside ?  1 : -1);

      return HGCSiliconDetId(det,zp,modulethickness,moduleplane,moduleu,modulev,celliu,celliv).rawId();
    }

    uint32_t getSiPMDetId(bool zside, int moduleplane, int modulev, int celliu, int celliv, int cellthickness) {
      int layer = moduleplane - 25;
      int type = (cellthickness == 2 ? 0 : cellthickness == 3 ? 1 : 2);
    
      int sipm = 1; // cast tiles
      if(moduleplane >= 42 && celliu >= 18) sipm = 2; // molded tiles

      int ring = (zside ? celliu : (-1)*celliu);
      int iphi = modulev*8 + celliv + 1;

      return HGCScintillatorDetId(type, layer, ring, iphi, false, sipm).rawId();
    }
  }
}
