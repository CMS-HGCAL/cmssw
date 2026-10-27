#include <iostream>
#include <iomanip>

// CMSSW imports
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"

// Alpaka imports
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/SynchronizingEDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToDevice.h"

// includes for data formats
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"
#include "DataFormats/HGCalDigi/interface/alpaka/HGCalSimHitAccumDevice.h"

// includes for size, calibration, and configuration parameters
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
    edm::EDGetTokenT<edm::PCaloHitContainer> g4HitsToken_;
    const device::EDPutToken<hgcaldigi::HGCalSimHitAccumDevice> g4AccumulatorToken_;
    int32_t ncellsMapped_;
  };

  //
  HGCalSimHitAccumulator::HGCalSimHitAccumulator(const edm::ParameterSet& iConfig)
      : SynchronizingEDProducer(iConfig),
        indexingToken_{esConsumes<>(iConfig.getParameter<edm::ESInputTag>("indexingSource"))},
        g4HitsToken_{consumes<edm::PCaloHitContainer>(iConfig.getParameter<edm::InputTag>("g4hits"))},
        g4AccumulatorToken_{produces()},
        ncellsMapped_(0) {
  }

  //
  void HGCalSimHitAccumulator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add("g4hits", edm::InputTag{})->setComment("Label for source of PCaloHits (Geant4)");
    desc.add("indexingSource", edm::ESInputTag{})->setComment("Label for cell dense indexer");
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

    auto const& g4Hits = iEvent.get(g4HitsToken_);
    std::cout << "Have " << g4Hits.size() << " hits" << std::endl;
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