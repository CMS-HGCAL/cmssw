#include "DPGAnalysis/HGCalNanoAOD/interface/HitInfoTableProducer.h"
#include "DataFormats/ForwardDetId/interface/HGCalDetId.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"
#include "DataFormats/ForwardDetId/interface/HGCSiliconDetId.h"

#include "DataFormats/CaloRecHit/interface/CaloRecHit.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"

#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/RecHitTools.h"
#include "DataFormats/HGCRecHit/interface/HGCRecHitCollections.h"

template <typename T>
class HGCalPositionFromDetIDTableProducer : public HitInfoTableProducer<edm::View<CaloRecHit>> {
 public:
  HGCalPositionFromDetIDTableProducer(edm::ParameterSet const& params)
    : HitInfoTableProducer(params),
    caloGeoToken_(esConsumes<edm::Transition::BeginRun>()) {
  }
  ~HGCalPositionFromDetIDTableProducer() override {}

  std::pair<int, int> wafer(const CaloRecHit& hit) {
    return tools_.getWafer(hit.detid());
  }
  
  std::pair<int, int> uv(const CaloRecHit& hit) {
    return tools_.getCell(hit.detid());
  }

  unsigned int siThickIndex(const CaloRecHit& hit) {
    return tools_.getSiThickness(hit.detid());
  }

  unsigned int layer(const CaloRecHit& hit) {
    return tools_.getLayer(hit.detid());
  }

  unsigned int zside(const CaloRecHit& hit) {
    return tools_.zside(hit.detid());
  }

  float eta(const CaloRecHit& hit) {
    return tools_.getEta(hit.detid());
  }

  float phi(const CaloRecHit& hit) {
    return tools_.getPhi(hit.detid());
  }

  void beginRun(const edm::Run&, const edm::EventSetup& iSetup) override {
    tools_.setGeometry(iSetup.getData(caloGeoToken_));
  }

 protected:
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeoToken_;
  hgcal::RecHitTools tools_;

};

#include "FWCore/Framework/interface/MakerMacros.h"
typedef HGCalPositionFromDetIDTableProducer<HGCRecHitCollection> HGCRecHitPositionTableProducer;
DEFINE_FWK_MODULE(HGCRecHitPositionTableProducer);
