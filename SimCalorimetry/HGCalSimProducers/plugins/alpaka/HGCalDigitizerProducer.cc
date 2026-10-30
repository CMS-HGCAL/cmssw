// CMSSW imports
#include "FWCore/AbstractServices/interface/RandomNumberGenerator.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include <iomanip>  // for std::setw
#include <future>
#include <chrono>

// CLHEP includes
#include "CLHEP/Random/RandomEngine.h"
#include "CLHEP/Random/RandFlat.h"

// Alpaka imports
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/SynchronizingEDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToDevice.h"

// includes for data formats
#include "DataFormats/HGCalDigi/interface/HGCalSimHitAccumHost.h"
#include "DataFormats/HGCalDigi/interface/alpaka/HGCalSimHitAccumDevice.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHost.h"
#include "DataFormats/HGCalDigi/interface/alpaka/HGCalDigiDevice.h"
#include "CondFormats/HGCalObjects/interface/HGCalAgeingCondsHost.h"
#include "CondFormats/DataRecord/interface/HGCalAgeingCondsRcd.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDevice.h"

//kernels handlers
#include "SimCalorimetry/HGCalSimProducers/interface/alpaka/HGCalDigitizerAlgorithms.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalDigitizerProducer : public stream::SynchronizingEDProducer<> {
  public:
    explicit HGCalDigitizerProducer(const edm::ParameterSet&);
    static void fillDescriptions(edm::ConfigurationDescriptions&);

  private:
    void acquire(device::Event const&, device::EventSetup const&) override;
    void produce(device::Event&, device::EventSetup const&) override;

    const edm::EDGetTokenT<hgcaldigi::HGCalSimHitAccumHost> g4HitsAccTkn_;
    edm::ESWatcher<HGCalAgeingCondsRcd> ageingCondsWatcher_;
    const edm::ESGetToken<hgcal::HGCalAgeingCondsHost, HGCalAgeingCondsRcd> ageingCondsTkn_;
    const edm::ESGetToken<hgcal::HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexTkn_;
    const edm::ESGetToken<hgcal::HGCalMappingCellParamHost, HGCalElectronicsMappingRcd> cellMappingToken_;
    const device::EDPutToken<hgcaldigi::HGCalROCDigisAccumDevice> digisAccTkn_;
    int n_blocks_, n_threads_;
    const HGCalAgeingAlgorithms ripener_;
    const HGCalROCDigitizerAlgorithms digitizer_;
    std::optional<hgcaldigi::HGCalRealisticSimHitDevice> agedG4HitsAcc_device_;
    std::optional<hgcaldigi::HGCalROCDigisAccumDevice> rocDigisAccum_device_;
  };

  //
  HGCalDigitizerProducer::HGCalDigitizerProducer(const edm::ParameterSet& iConfig)
      : SynchronizingEDProducer(iConfig),
        g4HitsAccTkn_{consumes<hgcaldigi::HGCalSimHitAccumHost>(iConfig.getParameter<edm::InputTag>("g4HitsAcc"))},
        ageingCondsTkn_{esConsumes(iConfig.getParameter<edm::ESInputTag>("ageingConds"))},
        denseIndexTkn_{esConsumes(iConfig.getParameter<edm::ESInputTag>("denseIndexer"))},
        cellMappingToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("cellInfo"))},
        digisAccTkn_{produces()},
        n_blocks_{iConfig.getParameter<int>("n_blocks")},
        n_threads_{iConfig.getParameter<int>("n_threads")},
        ripener_{n_blocks_, n_threads_},
        digitizer_{n_blocks_, n_threads_} {}

  //
  void HGCalDigitizerProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("g4HitsAcc", edm::InputTag("hgcalDigis", "DIGI", "TEST"));
    desc.add("ageingConds", edm::ESInputTag{})->setComment("Label for ageing parameters");
    desc.add("denseIndexer", edm::ESInputTag{})->setComment("Dense indexer SoA source");
    desc.add("cellInfo", edm::ESInputTag{})->setComment("cell info SoA source");
    desc.add<int>("n_blocks", -1);
    desc.add<int>("n_threads", -1);
    descriptions.addWithDefaultLabel(desc);
  }

  //
  void HGCalDigitizerProducer::acquire(device::Event const& iEvent, device::EventSetup const& iSetup) {
    auto& queue = iEvent.queue();
    const auto& ageingConds = iSetup.getData(ageingCondsTkn_);
    const auto& denseIndexer = iSetup.getData(denseIndexTkn_);
    const auto& cellInfo = iSetup.getData(cellMappingToken_);
    const auto& g4AccHits = iEvent.get(g4HitsAccTkn_);
    auto ncells = g4AccHits.view().metadata().size();

    LogDebug("") << "Loaded accumulated Geant 4 hits for " << ncells << " cells"
                 << ", " << ageingConds.view().metadata().size() << " ageing conditions"
                 << ", " << denseIndexer.view().metadata().size() << " dense indices"
                 << ", and " << cellInfo.view().metadata().size() << " cell info indices";

    //this should be commented out or put within pre-compiled #ifdef ML_DEBUG type of flags
    alpaka::wait(queue);
    auto start = std::chrono::steady_clock::now();
    //////////////////////////////////////////////

    //copy conditions and dense indexer to device
    hgcal::HGCalAgeingCondsDevice ageingConds_device(queue, ncells);
    alpaka::memcpy(queue, ageingConds_device.buffer(), ageingConds.const_buffer());
    hgcal::HGCalDenseIndexInfoDevice denseIndex_device(queue, ncells);
    alpaka::memcpy(queue, denseIndex_device.buffer(), denseIndexer.const_buffer());
    hgcal::HGCalMappingCellParamDevice cellInfo_device(queue, ncells);
    alpaka::memcpy(queue, cellInfo_device.buffer(), cellInfo.const_buffer());

    //run ageing
    agedG4HitsAcc_device_ = ripener_.age(queue, g4AccHits, ageingConds_device, denseIndex_device, cellInfo_device);

    //run digitization

    // setup RNG for noise generation
    edm::Service<edm::RandomNumberGenerator> rng;
    CLHEP::HepRandomEngine& engine = rng->getEngine(iEvent.streamID());
    const uint64_t seed = CLHEP::RandFlat::shootInt(&engine, std::numeric_limits<uint64_t>::max());

    if (agedG4HitsAcc_device_.has_value())
      rocDigisAccum_device_ = digitizer_.digitize(
          queue, agedG4HitsAcc_device_.value(), ageingConds_device, denseIndex_device, cellInfo_device, seed);

    //this should be commented out or put within pre-compiled #ifdef ML_DEBUG type of flags
    alpaka::wait(queue);
    auto stop = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = stop - start;
    LogDebug("HGCalDigitizerProducer") << "Time spent creating " << ncells << " digis: " << elapsed.count();
    //////////////////////////////////////////////
  }

  //
  void HGCalDigitizerProducer::produce(device::Event& iEvent, device::EventSetup const& iSetup) {
    //harvest digitized values
    //FIXME
    //auto digis = digitizer_.harvest(iEvent.queue(), *simHitAccum_device_);
    //hgcaldigi::HGCalROCDigisAccumDevice rocDigisAccum_device_(iEvent.queue(), 0);
    //alpaka::memcpy(queue, device_g4Hits.buffer(), host_g4Hits.const_buffer());

    LogDebug() << "Storing HGCAL DIGIs in the event";
    if (rocDigisAccum_device_.has_value()) {
      iEvent.emplace(digisAccTkn_, std::move(rocDigisAccum_device_.value()));
    } else {
      hgcaldigi::HGCalROCDigisAccumDevice empty_digis(iEvent.queue(), 0);
      iEvent.emplace(digisAccTkn_, std::move(empty_digis));
      edm::LogWarning("") << "Producing an empty set of accumulated ROC DIGIs";
    }
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalDigitizerProducer);
