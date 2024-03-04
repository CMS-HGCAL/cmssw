#include "DPGAnalysis/HGCalNanoAOD/interface/DigiInfoTableProducer.h"
#include "DataFormats/ForwardDetId/interface/HGCalDetId.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"
#include "DataFormats/ForwardDetId/interface/HGCSiliconDetId.h"

#include "DataFormats/CaloRecHit/interface/CaloRecHit.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"

#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/RecHitTools.h"
#include "DataFormats/HGCRecHit/interface/HGCRecHitCollections.h"

#include "DataFormats/HGCalDigi/interface/HGCalDigiCollections.h"

template <typename T1, typename T2>
class HGCalDigiFromDetIDTableProducer : public DigiInfoTableProducer< edm::View<CaloRecHit>,edm::View<HGCROCChannelDataFrameElecSpec> > {
 public:
  HGCalDigiFromDetIDTableProducer(edm::ParameterSet const& params)
    : DigiInfoTableProducer(params) {
  }
  ~HGCalDigiFromDetIDTableProducer() override {}

  uint32_t findMatchEleId(const CaloRecHit& hit, const std::map<uint32_t,uint32_t> & ele2geo_) {

    uint32_t eleId=-1;
    for(auto it : ele2geo_) {
      if (it.second==hit.detid().rawId()) eleId = it.first;
    }
    return eleId;
  }

  bool findMatch(const CaloRecHit& hit, const HGCROCChannelDataFrameElecSpec& digi, const std::map<uint32_t,uint32_t> & ele2geo_) {

    bool foundMatch = false;
    const HGCalElectronicsId  eleId = digi.id();
    for(auto it : ele2geo_) {
      if (it.first==eleId.raw() and it.second==hit.detid().rawId()) foundMatch = true;
    }
    return foundMatch;
  }

  uint32_t getEconDIdx(const HGCROCChannelDataFrameElecSpec& digi) {
    return digi.id().econdIdx();
  }

  uint32_t getEconDeRx(const HGCROCChannelDataFrameElecSpec& digi) {
    return digi.id().econdeRx();
  }

  uint32_t getHalfRocChannel(const HGCROCChannelDataFrameElecSpec& digi) {
    return digi.id().halfrocChannel();
  }

  uint32_t getElecIDisCM(const HGCROCChannelDataFrameElecSpec& digi) {
    return digi.id().isCM();
  }

  uint32_t getElecID(const HGCROCChannelDataFrameElecSpec& digi) {
    return digi.id().raw();
  }

  float getTOA(const HGCROCChannelDataFrameElecSpec& digi) {
    return digi.toa();
  }

  float getTOT(const HGCROCChannelDataFrameElecSpec& digi) {
    // FIXME: in the current HGCROC the calib channels (=18) is always in characterization model; to be fixed in ROCv3b
    return digi.tot(charMode_ || (fixCalibChannel_ && digi.id().halfrocChannel() == 18));
  }

  float getADC(const HGCROCChannelDataFrameElecSpec& digi) {
    // FIXME: in the current HGCROC the calib channels (=18) is always in characterization model; to be fixed in ROCv3b
    return digi.adc(charMode_ || (fixCalibChannel_ && digi.id().halfrocChannel() == 18));
  }

  float getADCm1(const HGCROCChannelDataFrameElecSpec& digi) {
    // FIXME: in the current HGCROC the calib channels (=18) is always in characterization model; to be fixed in ROCv3b
    return digi.adcm1(charMode_ || (fixCalibChannel_ && digi.id().halfrocChannel() == 18));
  }

  float getTctp(const HGCROCChannelDataFrameElecSpec& digi) {
    return digi.tctp();
  }

};

#include "FWCore/Framework/interface/MakerMacros.h"
typedef HGCalDigiFromDetIDTableProducer<HGCRecHitCollection,HGCalElecDigiCollection> HGCRecHitDigiTableProducer;
DEFINE_FWK_MODULE(HGCRecHitDigiTableProducer);
