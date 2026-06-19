// HGCal CM calibration using a PyTorch DNN via alpaka.
//
// Per-event workflow:
//   1. Consume host digis and copy to device.
//   2. Compute event-level ntoa/ntot on the host (cheap loop over digis).
//   3. Fill the ML input SoA on the device (one slot per global channel).
//   4. Run DNN inference:  inputs  → [cmsum tensor, features tensor]
//                          outputs → [correction tensor]
//   5. Apply per-channel additive correction to digi ADC in-place.
//
// Open-question assumptions encoded here:
//   - DNN model signature: forward(cmsum:[N,12], features:[N,6]) -> correction:[N]
//   - Correction is an additive float offset to raw digi ADC (uint16_t, clamped).
//   - cellfrac sourced from HGCalMappingCellParamSoA::trace().

#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EventSetup.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/EDProducer.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"

#include "DataFormats/HGCalDigi/interface/HGCalDigiHost.h"
#include "DataFormats/HGCalDigi/interface/alpaka/HGCalDigiDevice.h"

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDevice.h"

#include "PhysicsTools/PyTorchAlpaka/interface/TensorCollection.h"
#include "PhysicsTools/PyTorchAlpaka/interface/alpaka/AlpakaModel.h"

#include "RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCMCalibrationAlgorithms.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalCMCalibrationProducer : public stream::EDProducer<> {
  public:
    explicit HGCalCMCalibrationProducer(const edm::ParameterSet& iConfig);
    static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

  private:
    void produce(device::Event&, device::EventSetup const&) override;

    // --- tokens ---
    const edm::EDGetTokenT<hgcaldigi::HGCalDigiHost> digisToken_;
    const edm::EDPutTokenT<hgcaldigi::HGCalDigiHost> correctedDigisToken_;
    const edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexerToken_;
    const device::ESGetToken<hgcal::HGCalDenseIndexInfoDevice, HGCalDenseIndexInfoRcd> indexingToken_;
    const device::ESGetToken<hgcal::HGCalMappingCellParamDevice, HGCalElectronicsMappingRcd> cellmapToken_;

    // --- per-event device buffers for flat module arrays (rebuilt if ES changes) ---
    // These are re-uploaded every event for simplicity; a watcher can be added later.
    // Alternative: use an ESProducer to cache these on device — kept simple here.

    // --- algorithm and DNN model ---
    const HGCalCMCalibrationAlgorithms algo_;
    torch::AlpakaModel model_;
  };

  // ---------------------------------------------------------------------------
  HGCalCMCalibrationProducer::HGCalCMCalibrationProducer(const edm::ParameterSet& iConfig)
      : EDProducer<>(iConfig),
        digisToken_{consumes<hgcaldigi::HGCalDigiHost>(iConfig.getParameter<edm::InputTag>("digis"))},
        correctedDigisToken_{produces().produces<hgcaldigi::HGCalDigiHost>()},
        moduleIndexerToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("moduleIndexerSource"))},
        indexingToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("indexingSource"))},
        cellmapToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("cellmapSource"))},
        algo_{iConfig.getParameter<int>("n_threads")},
        model_{iConfig.getParameter<edm::FileInPath>("model").fullPath()} {}

  // ---------------------------------------------------------------------------
  void HGCalCMCalibrationProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("digis", edm::InputTag("hgcalDigis", "DIGI"))
        ->setComment("HGCal digi host collection");
    desc.add<edm::ESInputTag>("moduleIndexerSource", edm::ESInputTag(""))
        ->setComment("HGCalMappingModuleIndexer for channel/module layout");
    desc.add<edm::ESInputTag>("indexingSource", edm::ESInputTag(""))
        ->setComment("HGCalDenseIndexInfoDevice for per-channel mapping");
    desc.add<edm::ESInputTag>("cellmapSource", edm::ESInputTag(""))
        ->setComment("HGCalMappingCellParamDevice for cell area fraction");
    desc.add<edm::FileInPath>("model")->setComment("Path to TorchScript DNN model (.pth)");
    desc.add<int>("n_threads", 256)->setComment("Threads per alpaka block");
    descriptions.addWithDefaultLabel(desc);
  }

  // ---------------------------------------------------------------------------
  void HGCalCMCalibrationProducer::produce(device::Event& iEvent,
                                            device::EventSetup const& iSetup) {
    auto& queue = iEvent.queue();

    // ---- Retrieve conditions ----
    const auto& moduleIndexer = iSetup.getData(moduleIndexerToken_);
    const auto& deviceIndex = iSetup.getData(indexingToken_);
    const auto& deviceCellmap = iSetup.getData(cellmapToken_);

    // maxModulesCount() = total physical modules; maxModuleSize() = distinct typecodes (often 1).
    // Module indices (modOffsets_) run 0..maxModulesCount()-1, so size the arrays by count.
    const uint32_t nmodules = moduleIndexer.maxModulesCount();

    // ---- Build flat per-module arrays on host, copy to device ----
    // chDataOffsets[denseModIdx] = first global channel index for that module
    // enabledErx[denseModIdx]   = eRx bitmask for that module (bit e set → eRx e is active)
    std::vector<uint32_t> h_chDataOffsets(nmodules, 0u);
    std::vector<uint32_t> h_enabledErx(nmodules, 0u);

    for (const auto& fed : moduleIndexer.fedReadoutSequences()) {
      for (uint32_t modid = 0; modid < fed.chDataOffsets_.size(); ++modid) {
        uint32_t denseModIdx = moduleIndexer.getIndexForModule(fed.id, modid);
        if (denseModIdx >= nmodules)
          continue;
        h_chDataOffsets[denseModIdx] = fed.chDataOffsets_[modid];
        h_enabledErx[denseModIdx] = fed.enabledErx_[modid];
      }
    }

    auto d_chDataOffsets = make_device_buffer<uint32_t[]>(queue, nmodules);
    auto d_enabledErx = make_device_buffer<uint32_t[]>(queue, nmodules);
    alpaka::memcpy(queue, d_chDataOffsets, make_host_view(h_chDataOffsets.data(), nmodules));
    alpaka::memcpy(queue, d_enabledErx, make_host_view(h_enabledErx.data(), nmodules));

    // ---- Retrieve and copy digis to device ----
    const auto& hostDigis = iEvent.get(digisToken_);
    const uint32_t ndigis = hostDigis.view().metadata().size();
    hgcaldigi::HGCalDigiDevice deviceDigis(queue, ndigis);
    alpaka::memcpy(queue, deviceDigis.buffer(), hostDigis.const_buffer());

    // ---- Compute event-level ntoa / ntot on the host ----
    int ntoa = 0, ntot = 0;
    for (uint32_t i = 0; i < ndigis; ++i) {
      ntoa += (hostDigis.view()[i].toa() > 0) ? 1 : 0;
      ntot += (hostDigis.view()[i].tot() > 0) ? 1 : 0;
    }
    LogDebug("HGCalCMCalibrationProducer") << "ntoa=" << ntoa << " ntot=" << ntot
                                            << " ndigis=" << ndigis;

    // ---- Allocate ML input SoA on device and fill (one slot per digi) ----
    HGCalSoACMMLDeviceCollection deviceMLSoA(queue, ndigis);
    algo_.fillCMInputs(queue,
                       ndigis,
                       ntoa,
                       ntot,
                       deviceDigis,
                       deviceIndex,
                       deviceCellmap,
                       d_chDataOffsets.data(),
                       d_enabledErx.data(),
                       deviceMLSoA);

    // ---- DNN inference ----
    // All 21 input columns are adjacent float SOA_COLUMNs → contiguous [ndigis, 21] tensor.
    // Model: forward(features:[N,21]) -> correction:[N,1]  (subtractive: corrected = raw - pred)
    HGCalCMCorrectionDeviceCollection deviceCorrections(queue, ndigis);

    auto in_records = deviceMLSoA.const_view().records();
    auto out_records = deviceCorrections.view().records();

    const int batchSize = static_cast<int>(ndigis);

    cms::torch::alpakatools::TensorCollection<Queue> inputs{batchSize};
    inputs.template add<hgcalcmml::HGCalCMMLSoA>("features",
        in_records.cm0(),     in_records.cm1(),  in_records.cm2(),  in_records.cm3(),
        in_records.cm4(),     in_records.cm5(),  in_records.cm6(),  in_records.cm7(),
        in_records.cm8(),     in_records.cm9(),  in_records.cm10(), in_records.cm11(),
        in_records.msubchidx(),
        in_records.msuberxidx(),
        in_records.cellfrac(),
        in_records.unconn0(), in_records.unconn1(),
        in_records.unconn2(), in_records.unconn3(),
        in_records.ntoa(),
        in_records.ntot());

    cms::torch::alpakatools::TensorCollection<Queue> outputs{batchSize};
    outputs.template add<hgcalcmml::HGCalCMCorrectionSoA>("correction", out_records.correction());

    model_.forward(queue, inputs, outputs);

    // ---- Subtract DNN prediction from digi ADC in-place ----
    algo_.applyCMCorrections(queue, ndigis, deviceCorrections, deviceDigis);

    // ---- Copy corrected digis back to host and emplace into event ----
    // The host collection is allocated and the async memcpy is enqueued here.
    // The framework drains the alpaka queue after produce() returns, so the data
    // is valid by the time any downstream consumer accesses it.
    hgcaldigi::HGCalDigiHost correctedHostDigis(queue, ndigis);
    alpaka::memcpy(queue, correctedHostDigis.buffer(), deviceDigis.const_buffer());
    iEvent.emplace(correctedDigisToken_, std::move(correctedHostDigis));
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalCMCalibrationProducer);
