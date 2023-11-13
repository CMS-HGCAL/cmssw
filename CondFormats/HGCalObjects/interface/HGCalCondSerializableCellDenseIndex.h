// #ifndef CondFormats_HGCalObjects_HGCalCondSerializableCellDenseIndex_h
// #define CondFormats_HGCalObjects_HGCalCondSerializableCellDenseIndex_h

// #include <vector>
// #include <cstdint>
// #include <tuple>

// #include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
// #include "CondFormats/Serialization/interface/Serializable.h"

// /**
//    @short Dense Indexing Max values for module mapping
// */
// struct HGCalMappingDenseIndex {
//   uint32_t moduleTypeMax{1};          ///< maximum number of module types
//   uint32_t cellChipMax{6};                ///< maximum number of channel chips
//   uint32_t halfROCMax{6};             ///< maximum number of half ROC channels
//   uint32_t channelSeqMax{6};                 ///< maximum number sequence numbers
//   COND_SERIALIZABLE;
// };

// /**
//    @holder for the module mapping dense indexing
//  */
// class HGCalCondSerializableCellDenseIndex {
  
// public:

//   // typedef std::tuple<bool,int,int,int> ModuleInfoKey_t;
//   // typedef std::map<uint32_t, uint16_t> ERxBitPatternMap;
  
//   HGCalCondSerializableCellDenseIndex() {}
//   virtual ~HGCalCondSerializableCellDenseIndex() {}

//   uint32_t denseIndex(uint32_t type, uint32_t chip, uint32_t half, uint32_t seq) {
//     uint32_t rtn = type;
//     rtn = rtn * moduleTypeMax + chip;
//     rtn = rtn * cellChipMax + half;
//     rtn = rtn * halfROCMax + seq;
//     return rtn;
//   }

//   uint32_t getSize(bool roclevel=false) const{
//     uint32_t size = moduleTypeMax*cellChipMax*halfROCMax*channelSeqMax;
//     return size;
//   }

//   void setMaxValues(uint32_t typeMax, uint32_t chipMax, uint32_t halfMax, uint32_t seqMax){
//     moduleTypeMax = typeMax;
//     cellChipMax = chipMax;
//     halfROCMax = halfMax;
//     channelSeqMax = seqMax;
//   }

//   uint16_t convertType(std::string typeString) {
//     int letterAscii = (int)typeString[0];
//     int type;
//     if(typeString == "B11B12") {
//       type = std::stoi(std::to_string(letterAscii) + "11");
//     }
//     else if(typeString == "A5A6") {
//       type = std::stoi(std::to_string(letterAscii) + "5");
//     }
//     else {
//       type = std::stoi(std::to_string(letterAscii) + typeString.substr(1, typeString.size()-1));
//     }
//     return (uint16_t)type;
//   }
// }

// #endif
