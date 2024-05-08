#ifndef _geometry_hgcalmapping_hgcalmappingtools_h_
#define _geometry_hgcalmapping_hgcalmappingtools_h_

#include "DataFormats/ForwardDetId/interface/HGCSiliconDetId.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"

namespace hgcal {

  namespace mappingtools {

    uint16_t getEcondErx(uint16_t chip, uint16_t half);
    uint32_t getElectronicsId(
        bool zside, uint16_t fedid, uint16_t captureblock, uint16_t econdidx, int cellchip, int cellhalf, int cellseq);
    uint32_t getSiDetId(bool zside, int moduleplane, int moduleu, int modulev, int celltype, int celliu, int celliv);
    uint32_t getSiPMDetId(bool zside, int moduleplane, int modulev, int celltype, int celliu, int celliv);

    /**
     * @short matches the module and cell info by detId and returns their indices (-1 is used in case index was not found)
    */
    template<class T1,class T2>
    std::pair<int32_t,int32_t> getModuleCellIndicesForSiCell(const T1 &modules, const T2& cells, uint32_t detid) {

      std::pair<int32_t, int32_t> key(-1,-1);

      //get the module and cell parts of the id
      HGCSiliconDetId siid(detid);

      // match module det id
      uint32_t modid = siid.moduleId().rawId();
      for (int i = 0; i<modules.view().metadata().size(); i++) {
        auto imod = modules.view()[i];
        if(imod.detid() != modid) continue;

        key.first=i;

        //match cell by type of module and by cell det id
        DetId::Detector det(DetId::Detector::HGCalEE);
        uint32_t cellid = 0x3ff & HGCSiliconDetId(det, 0, 0, 0, 0, 0, siid.cellU(), siid.cellV()).rawId();
        for (int j = 0; j < cells.view().metadata().size(); j++) {
          auto jcell = cells.view()[j];
          if (jcell.typeidx() != imod.typeidx()) continue;
          if(jcell.detid() != cellid) continue;
          key.second=j;
          return key;
        }

        return key;
      }
      
      return key;
    }

    /**
     * @short after getting the module/cell indices it returns the sum of the corresponding electronics ids
    */
    template<class T1,class T2>
    uint32_t getElectronicsIdForSiCell(const T1 &modules, const T2& cells, uint32_t detid) {
      std::pair<int32_t,int32_t> idx =  getModuleCellIndicesForSiCell<T1,T2>(modules,cells,detid);
      if(idx.first<0 || idx.first<0) return 0;
      return modules.view()[idx.first].eleid() + cells.view()[idx.second].eleid();
    }
    
  }  // namespace mappingtools
}  // namespace hgcal

#endif
