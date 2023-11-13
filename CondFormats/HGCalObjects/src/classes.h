#include "CondFormats/HGCalObjects/src/headers.h"

namespace CondFormats_HGCalObjects {

  std::vector<int> v_i;
  std::pair<std::string, std::vector<int> > p_s_v_i;
  std::map<std::string, std::vector<int> > m_s_v_i;
  HGCalCondSerializableConfig h_csgc();
  
  HGCalSiCellChannelInfo hscci;
  std::vector<HGCalSiCellChannelInfo> v_hscci;
  HGCalCondSerializableSiCellChannelInfo h_cscci();

  HGCalSiPMTileInfo hsti;
  std::vector<HGCalSiPMTileInfo> v_hsti;
  HGCalCondSerializableSiPMTileInfo h_csti();

  HGCalModuleInfo hmi;
  std::vector<HGCalModuleInfo> v_hmi;
  HGCalCondSerializableModuleInfo h_csmi();

  HGCalPedestals hp;
  std::pair<uint32_t,HGCalPedestals> p_hp;
  std::map<uint32_t,HGCalPedestals> v_hp;
  HGCalCondSerializablePedestals h_csp();

  
}  // namespace CondFormats_HGCalObjects
