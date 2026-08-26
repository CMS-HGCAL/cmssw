// HGCal CM calibration using a PyTorch DNN via alpaka.
//
// Per-event workflow:
//   1. Consume host digis and copy to device.
//   2. Compute per-module ntoa/ntot on the host (cheap loop over digis).
//   3. Fill the ML input SoA on the device (one slot per global channel).
//   4. Run DNN inference:  inputs  → [cmsum tensor, features tensor]
//                          outputs → [correction tensor]
//   5. Apply per-channel additive correction to digi ADC in-place.
//
// Open-question assumptions encoded here:
//   - DNN model signature: forward(cmsum:[N,12], features:[N,6]) -> correction:[N]
//   - Correction is an additive float offset to raw digi ADC (uint16_t, clamped).
//   - cellfrac sourced from HGCalMappingCellParamSoA::trace().

#include <array>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

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
#include "DataFormats/HGCalDigi/interface/HGCalRawDataDefinitions.h"
#include "DataFormats/HGCalDigi/interface/alpaka/HGCalDigiDevice.h"
#include "DataFormats/Portable/interface/PortableHostCollection.h"
#include "DataFormats/Portable/interface/PortableHostCollection.h"

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalCalibrationParameterHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDevice.h"

#include "PhysicsTools/PyTorchAlpaka/interface/TensorCollection.h"
#include "PhysicsTools/PyTorchAlpaka/interface/alpaka/AlpakaModel.h"

#include "RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCMCalibrationAlgorithms.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCALSoACMML.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalCMCalibrationProducer : public stream::EDProducer<> {
  public:
    explicit HGCalCMCalibrationProducer(const edm::ParameterSet& iConfig);
    static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

  private:
    void produce(device::Event&, device::EventSetup const&) override;

    // Append one CSV row per (event, module, channel) that passes the debug filter,
    // dumping the 21 DNN input features plus IDs (event, denseModIdx, module typecode,
    // channel) and the raw digi_adc / unconn0_raw reference values. Host-side only.
    void writeDebugCsv(PortableHostCollection<hgcalcmml::HGCalCMMLSoA> const& mlsoa,
                       hgcaldigi::HGCalDigiHost const& digis,
                       std::vector<std::string> const& typecodes,
                       std::vector<uint32_t> const& chDataOffsets,
                       std::vector<uint32_t> const& enabledErx,
                       uint64_t eventNum,
                       uint32_t effDebugModule,
                       std::vector<float> const& dnnCorr) const;

    // --- tokens ---
    const edm::EDGetTokenT<hgcaldigi::HGCalDigiHost> digisToken_;
    const edm::EDPutTokenT<hgcaldigi::HGCalDigiHost> correctedDigisToken_;
    const edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> moduleIndexerToken_;
    const device::ESGetToken<hgcal::HGCalDenseIndexInfoDevice, HGCalDenseIndexInfoRcd> indexingToken_;
    const device::ESGetToken<hgcal::HGCalMappingCellParamDevice, HGCalElectronicsMappingRcd> cellmapToken_;
    const edm::ESGetToken<hgcalrechit::HGCalCalibParamHost, HGCalModuleConfigurationRcd> calibToken_;

    // --- per-event device buffers for flat module arrays (rebuilt if ES changes) ---
    // These are re-uploaded every event for simplicity; a watcher can be added later.
    // Alternative: use an ESProducer to cache these on device — kept simple here.

    // --- algorithm and DNN model ---
    const HGCalCMCalibrationAlgorithms algo_;
    torch::AlpakaModel model_;

    // --- cell area scale factors from cellareas.json, uploaded to device each event ---
    std::vector<float> h_sfLD_;  // ML_F SF, indexed by chIdx (222 entries, 6 eRx × 37)
    std::vector<float> h_sfHD_;  // MH_F SF, indexed by chIdx (444 entries, 12 eRx × 37)

    // --- debug print filter (0 = disabled for event; UINT32_MAX = all modules) ---
    // Module can be specified either as a dense index (debugModule) or by its
    // hardware address (debugFedId + debugCaptureBlock + debugEcond).  If all
    // three hardware fields are set (i.e. != UINT32_MAX), they take precedence.
    uint64_t debug_event_;
    uint32_t debug_module_;
    uint32_t debug_max_ch_;
    uint32_t debug_fed_id_;
    uint32_t debug_capblock_;
    uint32_t debug_econd_;

    // --- CSV debug dump (empty path = disabled) ---
    // The file is shared across stream instances; a mutex serializes appends and a
    // once_flag writes the header exactly once (truncating any file from a prior run).
    std::string debug_csv_path_;

    // --- optional float-correction output (for the CM-comparison DQM) ---
    // When on, emit the DNN's per-digi float prediction as a std::vector<float> (digi order),
    // so the comparison analyzer can plot the continuous correction (not just the quantized
    // integer ADC difference). Off by default; forces a queue sync, so avoid in production.
    bool emit_correction_;
    edm::EDPutTokenT<std::vector<float>> correctionToken_;  // valid only when emit_correction_
    // Emitted alongside it: the per-digi cell-area SF the kernels used. cellfrac == 0 marks an
    // unconnected channel, which applyCorrections deliberately skips -- the analyzer needs the
    // same flag to reproduce that treatment instead of re-deriving it from the cell mapping.
    edm::EDPutTokenT<std::vector<float>> cellfracToken_;  // valid only when emit_correction_

    // --- optional per-module CM pedestal override (empty path = disabled) ---
    // Arne Reimers pedestal subtractions are used in instead when we override, they can be different by an adc of around ~0.5 e.g. 191 versus 191.5
    struct CMPedOverride {
      std::array<float, 12> ped;  // half-scale CM_ped (cm_erxNN / 2)
      uint32_t nchadc;
      uint32_t nerx;
    };
    std::string cm_ped_override_path_;
    std::map<std::string, CMPedOverride> cm_ped_override_;  // typecode -> per-eRx pedestals

    static std::mutex csvMutex_;
    static std::once_flag csvInit_;
    static std::ofstream csvFile_;
    static unsigned csvWrites_;  // guarded by csvMutex_; drives the periodic flush
  };

  std::mutex HGCalCMCalibrationProducer::csvMutex_;
  std::once_flag HGCalCMCalibrationProducer::csvInit_;
  std::ofstream HGCalCMCalibrationProducer::csvFile_;
  unsigned HGCalCMCalibrationProducer::csvWrites_ = 0u;

  // ---------------------------------------------------------------------------
  HGCalCMCalibrationProducer::HGCalCMCalibrationProducer(const edm::ParameterSet& iConfig)
      : EDProducer<>(iConfig),
        digisToken_{consumes<hgcaldigi::HGCalDigiHost>(iConfig.getParameter<edm::InputTag>("digis"))},
        correctedDigisToken_{produces().produces<hgcaldigi::HGCalDigiHost>()},
        moduleIndexerToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("moduleIndexerSource"))},
        indexingToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("indexingSource"))},
        cellmapToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("cellmapSource"))},
        calibToken_{esConsumes(iConfig.getParameter<edm::ESInputTag>("calibSource"))},
        algo_{iConfig.getParameter<int>("n_threads")},
        model_{iConfig.getParameter<edm::FileInPath>("model").fullPath()},
        debug_event_{uint64_t(iConfig.getParameter<unsigned int>("debugEvent"))},
        debug_module_{iConfig.getParameter<unsigned int>("debugModule")},
        debug_max_ch_{iConfig.getParameter<unsigned int>("debugMaxChannels")},
        debug_fed_id_{iConfig.getParameter<unsigned int>("debugFedId")},
        debug_capblock_{iConfig.getParameter<unsigned int>("debugCaptureBlock")},
        debug_econd_{iConfig.getParameter<unsigned int>("debugEcond")},
        debug_csv_path_{iConfig.getParameter<std::string>("debugCsv")},
        emit_correction_{iConfig.getParameter<bool>("emitCorrection")},
        cm_ped_override_path_{iConfig.getParameter<std::string>("cmPedOverride")} {
    // Register the float-correction product only when requested. Use the alpaka adaptor form
    // (produces("label").produces<T>()) — the plain produces<T>("label") can't deduce the
    // Transition template parameter here.
    if (emit_correction_) {
      correctionToken_ = produces("correction").produces<std::vector<float>>();
      cellfracToken_ = produces("cellfrac").produces<std::vector<float>>();
    }
    // Load cell area scale factors from cellareas.json.
    // JSON structure: { "ML_F": { "SF": [...222 floats...] }, "MH_F": { "SF": [...444 floats...] } }
    std::string cellAreasPath = iConfig.getParameter<edm::FileInPath>("cellAreas").fullPath();
    std::ifstream cellAreasFile(cellAreasPath);
    if (!cellAreasFile.is_open())
      throw cms::Exception("Configuration") << "Cannot open cellareas file: " << cellAreasPath;
    nlohmann::json cellAreasJson;
    cellAreasFile >> cellAreasJson;
    h_sfLD_ = cellAreasJson.at("ML_F").at("SF").get<std::vector<float>>();
    h_sfHD_ = cellAreasJson.at("MH_F").at("SF").get<std::vector<float>>();
    LogDebug("HGCalCMCalibrationProducer")
        << "Loaded cellareas: ML_F=" << h_sfLD_.size() << " SF entries, "
        << "MH_F=" << h_sfHD_.size() << " SF entries";

    // Optional per-module CM pedestal override, in the DNN author's own format:
    //   { "ML-F3WC-IH0182": { "cm_erx00": 194.32644, ..., "cm_erx11": 0.0,
    //                         "nchadc": 222.0, "nerx": 6.0 }, ... }
    // cm_erxNN is FULL scale (= 2*CM_ped); halve on load so the kernel's half-scale
    // 2*(0.5*cm - CM_ped) reproduces cm_raw - cm_erxNN exactly.
    if (!cm_ped_override_path_.empty()) {
      std::ifstream cmPedFile(cm_ped_override_path_);
      if (!cmPedFile.is_open())
        throw cms::Exception("Configuration")
            << "Cannot open cmPedOverride file: " << cm_ped_override_path_;
      nlohmann::json cmPedJson;
      cmPedFile >> cmPedJson;
      for (auto const& [typecode, rec] : cmPedJson.items()) {
        CMPedOverride entry{};
        for (uint32_t e = 0; e < 12u; ++e) {
          char key[16];
          std::snprintf(key, sizeof(key), "cm_erx%02u", e);
          if (!rec.contains(key))
            throw cms::Exception("Configuration")
                << "cmPedOverride entry '" << typecode << "' is missing key '" << key
                << "'; all 12 cm_erxNN values are required";
          entry.ped[e] = 0.5f * rec.at(key).get<float>();
        }
        // nchadc/nerx are optional in the file but checked against the mapping when present.
        entry.nchadc = rec.contains("nchadc") ? uint32_t(rec.at("nchadc").get<double>()) : 0u;
        entry.nerx = rec.contains("nerx") ? uint32_t(rec.at("nerx").get<double>()) : 0u;
        cm_ped_override_[typecode] = entry;
      }
      edm::LogInfo("HGCalCMCalibrationProducer")
          << "Loaded CM pedestal override for " << cm_ped_override_.size() << " module(s) from "
          << cm_ped_override_path_;
    }
  }

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
    desc.add<edm::ESInputTag>("calibSource", edm::ESInputTag(""))
        ->setComment("HGCalCalibParamHost for per-channel ADC_ped and CM_ped");
    desc.add<edm::FileInPath>("model")->setComment("Path to TorchScript DNN model (.pth)");
    desc.add<edm::FileInPath>("cellAreas")->setComment("Path to cellareas.json with per-channel SF (ML_F/MH_F)");
    desc.add<int>("n_threads", 256)->setComment("Threads per alpaka block");
    desc.add<std::string>("cmPedOverride", "")
        ->setComment("Path to a JSON of per-module CM pedestals in the DNN author's convention "
                     "({typecode: {cm_erx00..cm_erx11, nchadc, nerx}}, cm_erxNN full-scale = "
                     "2*CM_ped). Overrides CM_ped from the calib file for the listed modules "
                     "only; ADC_ped is never touched. Empty = disabled.");
    desc.add<bool>("emitCorrection", false)
        ->setComment("also emit the per-digi float DNN correction as std::vector<float> (for CM comparison)");
    desc.add<unsigned int>("debugEvent", 0u)
        ->setComment("Event number to print DNN inputs for (0 = disabled)");
    desc.add<unsigned int>("debugModule", ~0u)
        ->setComment("Dense module index to filter (UINT32_MAX = all); overridden by debugFedId/CaptureBlock/Econd");
    desc.add<unsigned int>("debugMaxChannels", ~0u)
        ->setComment("Max within-module chIdx to print (UINT32_MAX = all; set to e.g. 5 for first 5 channels)");
    desc.add<unsigned int>("debugFedId", ~0u)
        ->setComment("FED ID of module to debug (must set all three hardware params to take effect)");
    desc.add<unsigned int>("debugCaptureBlock", ~0u)
        ->setComment("Capture block index of module to debug");
    desc.add<unsigned int>("debugEcond", ~0u)
        ->setComment("ECON-D index of module to debug");
    desc.add<std::string>("debugCsv", "")
        ->setComment("Path to write the 21 DNN inputs + IDs as CSV (empty = disabled). "
                     "Rows are filtered by debugEvent/debugModule/debugMaxChannels, same as the old debug print.");
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
    const auto& hostCalib = iSetup.getData(calibToken_);

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

    // ---- Build denseModIdx → typecode table (host-only, for the CSV debug dump) ----
    // The CSV tags each row with the human-readable module code (e.g. "ML-F3WC-IH0182")
    // so it can be filtered by module. typecodeMap() is host-only (typecode → fed/mod);
    // invert it via getIndexForModule(typecode) → denseModIdx. Only built when the CSV
    // dump or the CM pedestal override is enabled (both are keyed by typecode) to avoid the
    // string work on every event otherwise.
    std::vector<std::string> h_typecode;
    if (!debug_csv_path_.empty() || !cm_ped_override_.empty()) {
      h_typecode.assign(nmodules, std::string());
      for (const auto& kv : moduleIndexer.typecodeMap()) {
        uint32_t denseModIdx = moduleIndexer.getIndexForModule(kv.first);
        if (denseModIdx < nmodules)
          h_typecode[denseModIdx] = kv.first;
      }
    }

    // Upload cell area SF tables to device.
    auto d_sfLD = make_device_buffer<float[]>(queue, h_sfLD_.size());
    auto d_sfHD = make_device_buffer<float[]>(queue, h_sfHD_.size());
    alpaka::memcpy(queue, d_sfLD, make_host_view(h_sfLD_.data(), h_sfLD_.size()));
    alpaka::memcpy(queue, d_sfHD, make_host_view(h_sfHD_.data(), h_sfHD_.size()));

    // Upload per-channel pedestals (ADC_ped for unconn channels, CM_ped for CM sums) and the
    // per-channel validity flag. valid==0 marks a dead/uncalibrated channel: ADC_ped and
    // CM_ped are both 0 there, so any feature sourced from such a channel would carry a raw,
    // unsubtracted ADC/CM value. The RecHit kernel already guards on this flag; the ML path
    // must too, otherwise a single dead eRx poisons the module-level cm* features for every
    // channel of that module.
    const uint32_t ncalib = uint32_t(hostCalib.view().metadata().size());
    std::vector<float> h_adcPed(ncalib), h_cmPed(ncalib);
    std::vector<unsigned char> h_valid(ncalib);
    for (uint32_t i = 0; i < ncalib; ++i) {
      h_adcPed[i] = hostCalib.view()[i].ADC_ped();
      h_cmPed[i]  = hostCalib.view()[i].CM_ped();
      h_valid[i]  = hostCalib.view()[i].valid();
    }
    // ---- Optional per-module CM pedestal override ----
    // Replace CM_ped for the listed modules with the DNN author's own values (already halved
    // to the CM_ped convention on load). ADC_ped and valid are deliberately left alone: the
    // override record carries no ADC pedestal, and the validity guard must keep working.
    // Only enabled eRx are touched, so an absent eRx keeps the 0 the kernel expects.
    if (!cm_ped_override_.empty()) {
      for (uint32_t m = 0; m < nmodules; ++m) {
        if (m >= h_typecode.size() || h_typecode[m].empty())
          continue;
        auto it = cm_ped_override_.find(h_typecode[m]);
        if (it == cm_ped_override_.end())
          continue;
        const auto& ov = it->second;
        const uint32_t nErx = uint32_t(__builtin_popcount(h_enabledErx[m]));

        // Cross-check the record against the layout the mapping gives us. nchadc is the
        // whole-module channel count (222 on a 6-eRx LD module), not the per-eRx 37 -- the same
        // quantity that fixes the msubchidx mean-subtraction convention. A mismatch means the
        // record was built for a different module layout, and applying it would silently
        // corrupt the CM features.
        if (ov.nerx != 0u && ov.nerx != nErx)
          throw cms::Exception("Configuration")
              << "cmPedOverride '" << h_typecode[m] << "': nerx=" << ov.nerx
              << " but the mapping reports " << nErx << " enabled eRx";
        if (ov.nchadc != 0u && ov.nchadc != nErx * 37u)
          throw cms::Exception("Configuration")
              << "cmPedOverride '" << h_typecode[m] << "': nchadc=" << ov.nchadc
              << " but the mapping reports " << nErx * 37u << " channels (" << nErx
              << " eRx x 37)";

        const uint32_t chOffset = h_chDataOffsets[m];
        for (uint32_t e = 0; e < 12u; ++e) {
          if (!((h_enabledErx[m] >> e) & 1u))
            continue;
          if (ov.ped[e] == 0.0f)
            edm::LogWarning("HGCalCMCalibrationProducer")
                << "cmPedOverride '" << h_typecode[m] << "': eRx " << e
                << " is enabled but the override pedestal is 0 -- the raw CM sum will pass "
                   "through unsubtracted for this eRx";
          for (uint32_t k = 0; k < 37u; ++k) {
            const uint32_t idx = chOffset + e * 37u + k;
            if (idx < ncalib)
              h_cmPed[idx] = ov.ped[e];
          }
        }
        edm::LogInfo("HGCalCMCalibrationProducer")
            << "cmPedOverride applied to " << h_typecode[m] << " (denseModIdx " << m << "): "
            << "full-scale cm_erx = " << 2.f * ov.ped[0] << ", " << 2.f * ov.ped[1] << ", "
            << 2.f * ov.ped[2] << ", " << 2.f * ov.ped[3] << ", " << 2.f * ov.ped[4] << ", "
            << 2.f * ov.ped[5] << " ...";
      }
    }

    auto d_adcPed = make_device_buffer<float[]>(queue, ncalib);
    auto d_cmPed  = make_device_buffer<float[]>(queue, ncalib);
    auto d_valid  = make_device_buffer<unsigned char[]>(queue, ncalib);
    alpaka::memcpy(queue, d_adcPed, make_host_view(h_adcPed.data(), ncalib));
    alpaka::memcpy(queue, d_cmPed,  make_host_view(h_cmPed.data(),  ncalib));
    alpaka::memcpy(queue, d_valid,  make_host_view(h_valid.data(),  ncalib));

    // ---- Retrieve and copy digis to device ----
    const auto& hostDigis = iEvent.get(digisToken_);
    const uint32_t ndigis = hostDigis.view().metadata().size();
    hgcaldigi::HGCalDigiDevice deviceDigis(queue, ndigis);
    alpaka::memcpy(queue, deviceDigis.buffer(), hostDigis.const_buffer());

    // ---- Compute per-module ntoa / ntot on the host ----
    // Match the reference definition in DigiAnalysisUtils.py:
    //   ntot: flags!=NotAvailable && tctp==3 && tot>0   (genuine TOT-mode hits only)
    //   ntoa: flags!=NotAvailable && toa>0
    // Without the tctp==3 gate, ADC-mode channels with stale tot fields inflate ntot.
    //
    // Counts are accumulated per module (indexed by denseModIdx) rather than over the whole
    // event: each module's channels are contiguous in the digi SoA, starting at
    // chDataOffsets[m] and spanning popcount(enabledErx[m])*37 channels (nErx eRx × 37 ch).
    // The kernel then broadcasts d_ntoa[denseModIdx]/d_ntot[denseModIdx] to every slot of
    // that module.  This reuses the host arrays already built above; no digi→module lookup
    // or device atomics are needed.
    std::vector<int> h_ntoa(nmodules, 0), h_ntot(nmodules, 0);
    for (uint32_t m = 0; m < nmodules; ++m) {
      const uint32_t base = h_chDataOffsets[m];
      const uint32_t nch = uint32_t(__builtin_popcount(h_enabledErx[m])) * 37u;
      for (uint32_t i = base; i < base + nch && i < ndigis; ++i) {
        if (hostDigis.view()[i].flags() == ::hgcal::DIGI_FLAG::NotAvailable)
          continue;
        h_ntoa[m] += (hostDigis.view()[i].toa() > 0) ? 1 : 0;
        h_ntot[m] += (hostDigis.view()[i].tctp() == 3 && hostDigis.view()[i].tot() > 0) ? 1 : 0;
      }
    }
    auto d_ntoa = make_device_buffer<int[]>(queue, nmodules);
    auto d_ntot = make_device_buffer<int[]>(queue, nmodules);
    alpaka::memcpy(queue, d_ntoa, make_host_view(h_ntoa.data(), nmodules));
    alpaka::memcpy(queue, d_ntot, make_host_view(h_ntot.data(), nmodules));
    LogDebug("HGCalCMCalibrationProducer") << "computed per-module ntoa/ntot for "
                                           << nmodules << " modules, ndigis=" << ndigis;

    // Resolve hardware module address → dense index if all three fields are set.
    uint32_t eff_debug_module = debug_module_;
    if (debug_fed_id_ != ~0u && debug_capblock_ != ~0u && debug_econd_ != ~0u) {
      eff_debug_module = moduleIndexer.getIndexForModule(
          debug_fed_id_, uint16_t(debug_capblock_), uint16_t(debug_econd_));
      LogDebug("HGCalCMCalibrationProducer")
          << "debug: FED=" << debug_fed_id_ << " capblock=" << debug_capblock_
          << " econd=" << debug_econd_ << " → denseModIdx=" << eff_debug_module;
    }

    // ---- Allocate ML input SoA on device and fill (one slot per digi) ----
    HGCalSoACMMLDeviceCollection deviceMLSoA(queue, ndigis);
    algo_.fillCMInputs(queue,
                       ndigis,
                       d_ntoa.data(),
                       d_ntot.data(),
                       deviceDigis,
                       deviceIndex,
                       deviceCellmap,
                       d_chDataOffsets.data(),
                       d_enabledErx.data(),
                       d_sfLD.data(),
                       d_sfHD.data(),
                       d_adcPed.data(),
                       d_cmPed.data(),
                       d_valid.data(),
                       deviceMLSoA);

    // ---- Optional CSV dump of the 21 DNN inputs (debug) ----
    // Done host-side: copy the just-filled ML-input SoA back and, for each digi passing
    // the debug filter, write a CSV row. Same filter semantics as the old debug print:
    // debugEvent (0=off, UINT32_MAX=any), debugModule / hardware-resolved eff_debug_module
    // (UINT32_MAX=all), debugMaxChannels (max within-module chIdx). The alpaka::wait below
    // only runs when the dump is active, so the normal path stays fully asynchronous.
    const uint64_t eventNum = uint64_t(iEvent.id().event());
    const bool doCsv = !debug_csv_path_.empty() && debug_event_ != 0u &&
                       (debug_event_ == uint64_t(~uint32_t(0u)) || eventNum == debug_event_);
    // NOTE: the dump itself happens AFTER inference (below), so the CSV can carry the
    // producer's own DNN prediction next to the inputs that produced it -- that is what makes
    // the alpaka path checkable against an offline PyTorch evaluation of the same row.
    // deviceMLSoA is not modified by inference, so the inputs logged there are exactly what
    // the model was fed.

    // ---- DNN inference ----
    // All 21 input columns are adjacent float SOA_COLUMNs → contiguous [ndigis, 21] tensor.
    // Model: forward(features:[N,21]) -> correction:[N,1]  (subtractive: corrected = raw - pred)
    HGCalCMCorrectionDeviceCollection deviceCorrections(queue, ndigis);

    auto in_records = deviceMLSoA.const_view().records();
    auto out_records = deviceCorrections.view().records();

    const int batchSize = static_cast<int>(ndigis);

    cms::torch::alpakatools::TensorCollection<Queue> inputs{batchSize};
    // Argument order must match the SOA_COLUMN declaration order in HGCALSoACMML.h, which is
    // the model's training order (cm, nchtoa/nchtot, indices, cell area, unconnected).
    inputs.template add<hgcalcmml::HGCalCMMLSoA>("features",
        in_records.cm0(),     in_records.cm1(),  in_records.cm2(),  in_records.cm3(),
        in_records.cm4(),     in_records.cm5(),  in_records.cm6(),  in_records.cm7(),
        in_records.cm8(),     in_records.cm9(),  in_records.cm10(), in_records.cm11(),
        in_records.ntoa(),
        in_records.ntot(),
        in_records.msubchidx(),
        in_records.msuberxidx(),
        in_records.cellfrac(),
        in_records.unconn0(), in_records.unconn1(),
        in_records.unconn2(), in_records.unconn3());

    cms::torch::alpakatools::TensorCollection<Queue> outputs{batchSize};
    outputs.template add<hgcalcmml::HGCalCMCorrectionSoA>("correction", out_records.correction());

    model_.forward(queue, inputs, outputs);

    // ---- Optional CSV dump of the 21 DNN inputs + the resulting correction (debug) ----
    // Host-side: copy the ML-input SoA and the prediction back, then write one row per digi
    // passing the debug filter. Filter semantics: debugEvent (0=off, UINT32_MAX=any),
    // debugModule / hardware-resolved eff_debug_module (UINT32_MAX=all), debugMaxChannels.
    // The alpaka::wait only runs when the dump is active, so normal running stays async.
    if (doCsv) {
      PortableHostCollection<hgcalcmml::HGCalCMMLSoA> hostMLSoA(queue, ndigis);
      PortableHostCollection<hgcalcmml::HGCalCMCorrectionSoA> hostCorrCsv(queue, ndigis);
      alpaka::memcpy(queue, hostMLSoA.buffer(), deviceMLSoA.const_buffer());
      alpaka::memcpy(queue, hostCorrCsv.buffer(), deviceCorrections.const_buffer());
      alpaka::wait(queue);  // debug-only sync so the host copies are valid before reading
      std::vector<float> csvCorr(ndigis);
      auto ccv = hostCorrCsv.const_view();
      for (uint32_t i = 0; i < ndigis; ++i)
        csvCorr[i] = ccv[i].correction();
      writeDebugCsv(hostMLSoA, hostDigis, h_typecode, h_chDataOffsets, h_enabledErx,
                    eventNum, eff_debug_module, csvCorr);
    }

    // ---- Subtract DNN prediction from digi ADC in-place ----
    // deviceMLSoA carries the per-digi cellfrac so unconnected channels (SF==0) are skipped.
    algo_.applyCMCorrections(queue, ndigis, deviceCorrections, deviceMLSoA, deviceDigis);

    // ---- Copy corrected digis back to host and emplace into event ----
    // The host collection is allocated and the async memcpy is enqueued here.
    // The framework drains the alpaka queue after produce() returns, so the data
    // is valid by the time any downstream consumer accesses it.
    hgcaldigi::HGCalDigiHost correctedHostDigis(queue, ndigis);
    alpaka::memcpy(queue, correctedHostDigis.buffer(), deviceDigis.const_buffer());
    iEvent.emplace(correctedDigisToken_, std::move(correctedHostDigis));

    // ---- Optionally emit the DNN's float correction (for the CM-comparison DQM) ----
    // Copy the float correction SoA to host and expose it as a plain std::vector<float> in digi
    // order. Requires a queue sync to materialize the values before reading, so it only runs
    // when emitCorrection is set (i.e. in comparison mode), leaving normal running async.
    if (emit_correction_) {
      PortableHostCollection<hgcalcmml::HGCalCMCorrectionSoA> hostCorr(queue, ndigis);
      PortableHostCollection<hgcalcmml::HGCalCMMLSoA> hostMLOut(queue, ndigis);
      alpaka::memcpy(queue, hostCorr.buffer(), deviceCorrections.const_buffer());
      alpaka::memcpy(queue, hostMLOut.buffer(), deviceMLSoA.const_buffer());
      alpaka::wait(queue);
      std::vector<float> corrOut(ndigis), cellfracOut(ndigis);
      auto hcv = hostCorr.const_view();
      auto hmv = hostMLOut.const_view();
      for (uint32_t i = 0; i < ndigis; ++i) {
        corrOut[i] = hcv[i].correction();
        cellfracOut[i] = hmv[i].cellfrac();
      }
      iEvent.emplace(correctionToken_, std::move(corrOut));
      iEvent.emplace(cellfracToken_, std::move(cellfracOut));
    }
  }

  // ---------------------------------------------------------------------------
  void HGCalCMCalibrationProducer::writeDebugCsv(
      PortableHostCollection<hgcalcmml::HGCalCMMLSoA> const& mlsoa,
      hgcaldigi::HGCalDigiHost const& digis,
      std::vector<std::string> const& typecodes,
      std::vector<uint32_t> const& chDataOffsets,
      std::vector<uint32_t> const& enabledErx,
      uint64_t eventNum,
      uint32_t effDebugModule,
      std::vector<float> const& dnnCorr) const {
    const uint32_t ndigis = uint32_t(digis.view().metadata().size());
    const uint32_t nmodules = uint32_t(typecodes.size());
    auto dv = digis.view();
    auto sv = mlsoa.const_view();
    // Unconnected-channel positions within an eRx (same as the kernel); unconn0 is echoed
    // raw (pre-pedestal) as a cross-check column next to the pedestal-subtracted features.
    constexpr uint32_t kUnconn0Pos = 8u;

    // Format the whole event into a local buffer FIRST, with no lock held: snprintf is the
    // expensive part and it parallelises across streams. Only the final append is serialised.
    // (Previously the mutex wrapped the formatting loop too, so the CSV dump single-threaded
    // the whole job, and the per-event flush forced a write syscall per event.)
    std::string buf;
    buf.reserve(4096);
    char line[768];

    for (uint32_t m = 0; m < nmodules; ++m) {
      if (effDebugModule != ~0u && m != effDebugModule)
        continue;
      const uint32_t base = chDataOffsets[m];
      const uint32_t nch = uint32_t(__builtin_popcount(enabledErx[m])) * 37u;
      buf.reserve(buf.size() + size_t(nch) * 256);
      for (uint32_t i = base; i < base + nch && i < ndigis; ++i) {
        const uint32_t chIdx = i - base;
        if (chIdx >= debug_max_ch_)
          continue;
        const uint32_t erxIdx = chIdx / 37u;
        const float digi_adc = float(dv[i].adc());
        const float unconn0_raw = float(dv[base + erxIdx * 37u + kUnconn0Pos].adc());
        auto s = sv[i];
        // %g reproduces the previous ostream default (6 significant digits) exactly.
        int n = std::snprintf(
            line, sizeof(line),
            "%llu,%u,%s,%u,%g,%g,"
            "%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,"
            "%g,%g,%g,%g,%g,%g,%g,%g,%g",
            static_cast<unsigned long long>(eventNum), m, typecodes[m].c_str(), chIdx,
            digi_adc, unconn0_raw,
            s.cm0(), s.cm1(), s.cm2(), s.cm3(), s.cm4(), s.cm5(),
            s.cm6(), s.cm7(), s.cm8(), s.cm9(), s.cm10(), s.cm11(),
            s.msubchidx(), s.msuberxidx(), s.cellfrac(),
            s.unconn0(), s.unconn1(), s.unconn2(), s.unconn3(), s.ntoa(), s.ntot());
        if (n > 0)
          buf.append(line, size_t(n));
        // Raw CM sums per eRx: digi.cm() of the first channel of each eRx, with NO pedestal
        // subtraction and no scaling. Disabled eRx write 0, matching the kernel's
        // encoding for the cm* feature columns. Relation to the feature column:
        //   cm_e = 2 * (0.5 * cm_e_raw - CM_ped_e) = cm_e_raw - 2*CM_ped_e
        // so CM_ped_e can be recovered per eRx as 0.5*(cm_e_raw - cm_e).
        for (uint32_t e = 0; e < 12u; ++e) {
          const bool erxOn = (enabledErx[m] >> e) & 1u;
          const uint32_t src = base + e * 37u;
          n = std::snprintf(line, sizeof(line), ",%g",
                            (erxOn && src < ndigis) ? float(dv[src].cm()) : 0.0f);
          if (n > 0)
            buf.append(line, size_t(n));
        }
        // The producer's own prediction for this digi: lets an offline PyTorch run on the
        // same 21 inputs be compared row by row against the alpaka inference path.
        n = std::snprintf(line, sizeof(line), ",%.9g", i < dnnCorr.size() ? dnnCorr[i] : 0.0f);
        if (n > 0)
          buf.append(line, size_t(n));
        buf.push_back('\n');
      }
    }
    if (buf.empty())
      return;

    std::lock_guard<std::mutex> lock(csvMutex_);
    std::call_once(csvInit_, [this] {
      csvFile_.open(debug_csv_path_, std::ios::out | std::ios::trunc);
      csvFile_ << "event,denseModIdx,module,ch,digi_adc,unconn0_raw,"
                  "cm0,cm1,cm2,cm3,cm4,cm5,cm6,cm7,cm8,cm9,cm10,cm11,"
                  "msubchidx,msuberxidx,cellfrac,unconn0,unconn1,unconn2,unconn3,ntoa,ntot,"
                  "cm0_raw,cm1_raw,cm2_raw,cm3_raw,cm4_raw,cm5_raw,"
                  "cm6_raw,cm7_raw,cm8_raw,cm9_raw,cm10_raw,cm11_raw,dnn_corr\n";
    });
    if (!csvFile_.is_open())
      return;
    csvFile_.write(buf.data(), std::streamsize(buf.size()));
    // Flush periodically rather than every event: the stream is flushed on close at job end,
    // and an occasional flush keeps a running job's file usable without a syscall per event.
    if (++csvWrites_ % 512u == 0u)
      csvFile_.flush();
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalCMCalibrationProducer);
