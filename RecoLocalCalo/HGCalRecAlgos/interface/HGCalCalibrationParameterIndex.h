#ifndef RecoLocalCalo_HGCalRecAlgos_HGCalCalibrationParameterIndex_h
#define RecoLocalCalo_HGCalRecAlgos_HGCalCalibrationParameterIndex_h

#include <cstdint>
#include <vector>
#include "CondFormats/HGCalObjects/interface/HGCalCondSerializableModuleInfo.h"
#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h" // for HGCalElectronicsIdMask, HGCalElectronicsIdShift

struct HGCalCalibrationParameterIndex {
    uint32_t eventSLinkMax{1000};         ///< maximum number of S-Links in one Event
    uint32_t sLinkCaptureBlockMax{10};    ///< maximum number of capture blocks in one S-Link
    uint32_t captureBlockECONDMax{12};    ///< maximum number of ECON-Ds in one capture block
    uint32_t econdERXMax{12};             ///< maximum number of eRxs in one ECON-D
    uint32_t erxChannelMax{37};           ///< maximum number of channels in one eRx

    //enum HGCalElectronicsIdMask {
    //    kZsideMask = 0x1,
    //    kFEDIDMask = 0x3ff,
    //    kCaptureBlockMask = 0xf,
    //    kECONDIdxMask = 0xf,
    //    kECONDeRxMask = 0xf,
    //    kHalfROCChannelMask = 0x3f
    //};

    //enum HGCalElectronicsIdShift {
    //    kZsideShift = 28,
    //    kFEDIDShift = 18,
    //    kCaptureBlockShift = 14,
    //    kECONDIdxShift = 10,
    //    kECONDeRxShift = 6,
    //    kHalfROCChannelShift = 0
    //};

    constexpr uint32_t denseMap(uint32_t elecID) const{ // for channel-level parameter SoA
        uint32_t sLink = ((elecID >> HGCalElectronicsId::kFEDIDShift) & HGCalElectronicsId::kFEDIDMask);
        uint32_t captureBlock = ((elecID >> HGCalElectronicsId::kCaptureBlockShift) & HGCalElectronicsId::kCaptureBlockMask);
        uint32_t econd = ((elecID >> HGCalElectronicsId::kECONDIdxShift) & HGCalElectronicsId::kECONDIdxMask);
        uint32_t eRx = ((elecID >> HGCalElectronicsId::kECONDeRxShift) & HGCalElectronicsId::kECONDeRxMask);
        uint32_t channel = ((elecID >> HGCalElectronicsId::kHalfROCChannelShift) & HGCalElectronicsId::kHalfROCChannelMask);
        uint32_t rtn = sLink * sLinkCaptureBlockMax + captureBlock;
        rtn = rtn * captureBlockECONDMax + econd;
        rtn = rtn * econdERXMax + eRx;
        rtn = rtn * erxChannelMax + channel;
        return rtn;
    }

    constexpr uint32_t denseROCMap(uint32_t elecID) const{ // for ROC-level parameter SoA
        uint32_t sLink = ((elecID >> HGCalElectronicsId::kFEDIDShift) & HGCalElectronicsId::kFEDIDMask);
        uint32_t captureBlock = ((elecID >> HGCalElectronicsId::kCaptureBlockShift) & HGCalElectronicsId::kCaptureBlockMask);
        uint32_t econd = ((elecID >> HGCalElectronicsId::kECONDIdxShift) & HGCalElectronicsId::kECONDIdxMask);
        uint32_t eRx = ((elecID >> HGCalElectronicsId::kECONDeRxShift) & HGCalElectronicsId::kECONDeRxMask);
        uint32_t rtn = sLink * sLinkCaptureBlockMax + captureBlock;
        rtn = rtn * captureBlockECONDMax + econd;
        rtn = rtn * econdERXMax + eRx;
        return rtn;
    }

    void setMaxValues(const HGCalCondSerializableModuleInfo moduleInfo, const int commonMode=2) {
        std::tuple<uint16_t,uint8_t,uint8_t,uint8_t> denseIdxMax = moduleInfo.getMaxValuesForDenseIndex();
        eventSLinkMax        = std::get<0>(denseIdxMax);
        sLinkCaptureBlockMax = std::get<1>(denseIdxMax);
        captureBlockECONDMax = std::get<2>(denseIdxMax);
        econdERXMax          = std::get<3>(denseIdxMax);
        erxChannelMax        = 37 + commonMode; // +2 for the two common modes
        //std::cout << "HGCalCalibrationParameterIndex"
        //  << ": eventSLinkMax=" << cpi.eventSLinkMax << ", sLinkCaptureBlockMax=" << cpi.sLinkCaptureBlockMax
        //  << ", captureBlockECONDMax=" << cpi.captureBlockECONDMax << ", econdERXMax=" << cpi.econdERXMax
        //  << ", erxChannelMax=" << cpi.erxChannelMax << std::endl;
    }

    constexpr uint32_t getSize(bool roclevel=false) const{
        uint32_t size = eventSLinkMax*sLinkCaptureBlockMax*captureBlockECONDMax*econdERXMax;
        if(!roclevel) // channel-level: include channel size
          size *= erxChannelMax;
        return size;
    }

};

#endif
