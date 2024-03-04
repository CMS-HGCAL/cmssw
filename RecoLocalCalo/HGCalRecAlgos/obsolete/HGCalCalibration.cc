#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalCalibration.h"

HGCalCalibration::HGCalCalibration(){
    //TODO: when parameter provider ready, use it here
}

HGCalDetId HGCalCalibration::logicalMapping(HGCalElectronicsId elecID){
    return HGCalDetId(elecID.raw());

}

float HGCalCalibration::ToA2Time(uint16_t ToA, HGCalElectronicsId elecID){
    return float(ToA);
}

float HGCalCalibration::ADC2Energy(uint16_t ADC, HGCalElectronicsId elecID){
    return float(ADC);
}

float HGCalCalibration::ToT2Energy(uint16_t ToT, HGCalElectronicsId elecID){
    return float(ToT);
}

float HGCalCalibration::pedestalCorrection(HGCalElectronicsId elecID){
    return 0.f;
}

float HGCalCalibration::CMCorrection(uint16_t cm){
    return 0.f;
}

float HGCalCalibration::ADCmCorrection(uint16_t adcm, HGCalElectronicsId elecID){
    return 0.f;
}

HGCRecHit HGCalCalibration::calibrate(HGCROCChannelDataFrame<HGCalElectronicsId> elecDigi, uint16_t flag){
    HGCalElectronicsId elecID = elecDigi.id();
    uint16_t cm=0;
    uint16_t adcm = elecDigi.adcm1();
    uint16_t adc = elecDigi.adc();
    uint16_t tot = elecDigi.tot();
    uint16_t toa = elecDigi.toa();

    HGCalDetId DetId = logicalMapping(elecID);
    float energy = ADC2Energy(adc,elecID);
    float time = ToA2Time(toa,elecID);    

    float correction=0.f;

    if ((flag >> kPedestalCorrection) & 1)
        correction += pedestalCorrection(elecID);    
    if ((flag >> kCMCorrection) & 1)
        correction += CMCorrection(cm);
    if ((flag >> kADCmCorrection) & 1 )
        correction += ADCmCorrection(adcm,elecID);
    energy -= correction;
    //HGCRecHit(const DetId& id,float energy,float time,uint32_t flags = 0,uint32_t flagBits = 0,uint8_t son = 0,float timeError = 0.f);
    return HGCRecHit(DetId,energy,time,flag,0,0,0.f);
}
