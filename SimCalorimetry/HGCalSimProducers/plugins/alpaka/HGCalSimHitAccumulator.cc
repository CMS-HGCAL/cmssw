#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>

// framework imports
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"

// alpaka imports
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/SynchronizingEDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToDevice.h"

// data formats
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"
#include "DataFormats/HGCalDigi/interface/alpaka/HGCalSimHitAccumDevice.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDevice.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalSimHitAccumulator : public stream::SynchronizingEDProducer<> {
  public:
    explicit HGCalSimHitAccumulator(const edm::ParameterSet&);

    /**
    @short declares configuration parameters allowed
    */
    static void fillDescriptions(edm::ConfigurationDescriptions&);

  private:

    void acquire(device::Event const&, device::EventSetup const&) override;
    void produce(device::Event&, device::EventSetup const&) override;

    /**
    @short reads the electronics dense indexing and builds an internal DetId -> DenseIndex mapping
    The mapping is only built once in a run. The `ncellsMapped_` variable is used to track if the map is available or not
    */
    void buildGeoToDenseIndexMap(const hgcal::HGCalDenseIndexInfoDevice &denseIndexInfo);
    

    const device::ESGetToken<hgcal::HGCalDenseIndexInfoDevice, HGCalDenseIndexInfoRcd> indexingToken_;
    std::map<std::string, edm::EDGetTokenT<edm::PCaloHitContainer> > g4HitsTokens_;
    const device::EDPutToken<hgcaldigi::HGCalSimHitAccumDevice> g4AccumulatorToken_;
    int32_t ncellsMapped_;
  };

  //
  HGCalSimHitAccumulator::HGCalSimHitAccumulator(const edm::ParameterSet& iConfig)
      : SynchronizingEDProducer(iConfig),
        indexingToken_{esConsumes<>(iConfig.getParameter<edm::ESInputTag>("indexingSource"))},
        g4HitsTokens_{},
        g4AccumulatorToken_{produces()},
        ncellsMapped_(0) {

    //Geant4 hit sources to handle
    for (auto const& tag : iConfig.getParameter<std::vector<edm::InputTag>>("g4Hits")) {
      auto k = tag.instance();
      g4HitsTokens_[ k ] = consumes<edm::PCaloHitContainer>(tag);
      std::cout << "Reading: " << k << std::endl;
    }

  }

  //
  void HGCalSimHitAccumulator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add< std::vector<edm::InputTag> >("g4Hits")->setComment("Label for source of PCaloHits (Geant4)");
    desc.add<edm::ESInputTag>("indexingSource")->setComment("Label for cell dense indexer");
    descriptions.addWithDefaultLabel(desc);
  }

  //
  void HGCalSimHitAccumulator::buildGeoToDenseIndexMap(const hgcal::HGCalDenseIndexInfoDevice &denseIndexInfo) {
    if(ncellsMapped_>0) return;
    auto const& denseIndexInfo_view = denseIndexInfo.const_view();
    int32_t ndenseIndices = denseIndexInfo_view.metadata().size();
    std::cout << "Have " << ndenseIndices << " to map!" << std::endl;
    ncellsMapped_=ndenseIndices;
  }

  /**
  @short
   */
  void HGCalSimHitAccumulator::acquire(device::Event const& iEvent, device::EventSetup const& iSetup) {
    auto& queue = iEvent.queue();

    auto const& denseIndexInfo = iSetup.getData(indexingToken_);
    buildGeoToDenseIndexMap(denseIndexInfo);

    std::size_t totalHits = 0;
    for (auto const& g4It : g4HitsTokens_) {
      const auto k = g4It.first;
      const auto token = g4It.second;
      const auto& hits = iEvent.get(token);
      std::cout << "\t" << k << " has " << hits.size() << std::endl;
      totalHits += hits.size();
    }
    std::cout << "Have " << totalHits << " hits" << std::endl;
  }

  /**
   */
  void HGCalSimHitAccumulator::produce(device::Event& iEvent, device::EventSetup const& iSetup) {

    auto& queue = iEvent.queue();
    hgcaldigi::HGCalSimHitAccumDevice g4Accumulators(queue, ncellsMapped_);
    LogDebug("HGCalSimHitAccumulator") << "Storing accumulators for " << ncellsMapped_ << " in the event";
    iEvent.emplace(g4AccumulatorToken_, std::move(g4Accumulators));
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

// define this as a plug-in
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalSimHitAccumulator);