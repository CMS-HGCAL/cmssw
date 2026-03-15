#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"
#include "DataFormats/HGCalDigi/interface/HGCalSimHitAccumSoA.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/StreamID.h"

class HGCalSimHitAccumulator : public edm::stream::EDProducer<> {

public:
  explicit HGCalSimHitAccumulator(const edm::ParameterSet&);

  void produce(edm::Event&, const edm::EventSetup&) override;

private:

  edm::EDGetTokenT<edm::PCaloHitContainer> simHitToken_;
  float timeBin_;
};

//
HGCalSimHitAccumulator::HGCalSimHitAccumulator(
    const edm::ParameterSet& ps)
{

  simHitToken_ =
      consumes<edm::PCaloHitContainer>(
          ps.getParameter<edm::InputTag>("simHits"));

  timeBin_ = ps.getParameter<double>("timeBin");

  produces<hgcal::HGCalSimHitAccumSoA>();
}

//
void HGCalSimHitAccumulator::produce(edm::Event& evt,
                                    const edm::EventSetup&)
{
  auto const& simHits = evt.get(simHitToken_);

  //build a compact list of hits to be accumulated
  std::vector<hgcaldigi::G4HitLite> hits;
  hits.reserve(simHits.size());
  for (auto const& h : simHits) {
    float t = h.time();
    int bin = int(t / timeBin_);
    if (bin < 0 || bin >=  hgcaldigi::kTimeBins;)
      continue;
    hits.push_back({h.id(), h.energy(), t, bin});
  }

  // sort by index
  std::sort(
      hits.begin(), hits.end(),
      [](auto const& a, auto const& b) {
        return a.idx < b.idx;
      });

  // start SOA (need module locator)
  size_t rows = 1000;
  auto soa = std::make_unique<hgcal::HGCalSimHitAccumSoA>(rows);
  
  bool first = true;
  uint32_t currentIdx = hits[0].idx;
  hgcaldigi::TimedAccumulatorF sumE, sumExT;
  for (auto const& h : hits) {
    
    if (first || h.idx != currentIdx) {

      if (!first) {
        soa->sumE(currentIdx) = sumE;
        soa->sumExT(currentIdx) = sumExT;
      }

      currentIdx = h.detId;
      sumE.setZero();
      sumExT.setZero();
      first = false;
    }

    sumE(h.idx)  += h.energy;
    sumExT(h.idx) += h.energy * h.time;
  }

  //conclude for the last hit
  if (!first) {
    soa->sumE(currentIdx) = sumE;
    soa->sumExT(currentIdx) = sumExT;
  }

  //all done, move to event
  evt.put(std::move(soa));
}
