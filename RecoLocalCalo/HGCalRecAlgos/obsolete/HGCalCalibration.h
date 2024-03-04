#ifndef RecoLocalCalo_HGCalRecAlgos_HGCalCalibration_h
#define RecoLocalCalo_HGCalRecAlgos_HGCalCalibration_h

#include <map>

#include "DataFormats/HGCalDigi/interface/HGCalElectronicsId.h"
#include "DataFormats/HGCalDigi/interface/HGCROCChannelDataFrame.h"

#include "DataFormats/HGCRecHit/interface/HGCRecHit.h"
#include "DataFormats/ForwardDetId/interface/HGCalDetId.h"


class HGCalCalibration{
    public:
    enum HGCalCalibrationFlag {
        kPedestalCorrection=0,
        kCMCorrection,
        kADCmCorrection,
    };
    HGCalDetId logicalMapping(HGCalElectronicsId elecID);
    float ToA2Time(uint16_t ToA, HGCalElectronicsId elecID);
    float ADC2Energy(uint16_t ADC, HGCalElectronicsId elecID);
    float ToT2Energy(uint16_t ToT, HGCalElectronicsId elecID);
    float pedestalCorrection(HGCalElectronicsId elecID);
    float CMCorrection(uint16_t cm);
    float ADCmCorrection(uint16_t adcm,HGCalElectronicsId elecID);
    HGCRecHit calibrate(HGCROCChannelDataFrame<HGCalElectronicsId> elecDigi, uint16_t flag);
    HGCalCalibration();
    //provider
    private:
    std::map<HGCalElectronicsId, float> map_pedestal_;
    std::map<HGCalElectronicsId, float> map_cm_correction_;
    std::map<HGCalElectronicsId, float> map_ADC_conversion_;
    std::map<HGCalElectronicsId, float> map_ToT_conversion_;
    //more map for parameters
};

#endif
