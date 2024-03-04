// #include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalCalibrationParameterIndex.h"
// #include "FWCore/MessageLogger/interface/MessageLogger.h"
//
// void HGCalCalibrationParameterIndex::initialize(HGCalCalibrationParameterIndexConfig config){
//     config_=config;
//     LogDebug("HGCalCalibrationParameterIndex")<<"initializing with EventSLinkMax="<<config_.EventSLinkMax
//                                                  <<",sLinkCaptureBlockMax="<<config_.sLinkCaptureBlockMax
//                                                  <<",captureBlockECONDMax="<<config_.captureBlockECONDMax
//                                                  <<",econdERXMax="<<config_.econdERXMax
//                                                  <<",erxChannelMax="<<config_.erxChannelMax;
//     calibrationParameter_=std::vector<CalibrationParameter>(config_.EventSLinkMax*config_.sLinkCaptureBlockMax*config_.captureBlockECONDMax*config_.econdERXMax*config_.erxChannelMax);
// }
//
// const uint32_t HGCalCalibrationParameterIndex::denseMap(uint32_t ElectronicsID) const{
//     uint32_t sLink = ((ElectronicsID >> kFEDIDShift) & kFEDIDMask);
//     uint32_t captureBlock = ((ElectronicsID >> kCaptureBlockShift) & kCaptureBlockMask);
//     uint32_t econd = ((ElectronicsID >> kECONDIdxShift) & kECONDIdxMask);
//     uint32_t eRx = ((ElectronicsID >> kECONDeRxShift) & kECONDeRxMask);
//     uint32_t channel = ((ElectronicsID >> kHalfROCChannelShift) & kHalfROCChannelMask);
//     uint32_t rtn = sLink * config_.sLinkCaptureBlockMax + captureBlock;
//     rtn = rtn * config_.captureBlockECONDMax + econd;
//     rtn = rtn * config_.econdERXMax + eRx;
//     rtn = rtn * config_.erxChannelMax + channel;
//     return rtn;
// }
//
// CalibrationParameter& HGCalCalibrationParameterIndex::operator[](uint32_t ElectronicsID){
//     return calibrationParameter_[denseMap(ElectronicsID)];
// }
