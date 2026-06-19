# HGCal Common-Mode Calibration via PyTorch DNN (alpaka)

This document covers every file created or modified to implement the common-mode (CM) noise
correction for HGCal digis using a PyTorch DNN running on device (GPU or CPU) via the CMSSW
alpaka portability layer. C++ lives in `RecoLocalCalo/HGCalRecAlgos`; Python wiring and the
model file live in `HGCalCommissioning/`.

---

## Overview

HGCal HGCROC ASICs read out a common-mode (CM) voltage per eRx that correlates with pedestal
noise on ordinary channels. This implementation runs a per-channel DNN correction that takes
raw CM sums, channel geometry, unconnected-channel ADCs, and event-level timing occupancy as
input, then **subtracts** the predicted noise from the raw digi ADC before the RecHit step.

Correction convention (confirmed with model author Arne Reimers):
```
corrected_adc = raw_adc - model_prediction
```

Per-event workflow:
```
hgcalDigis (HGCalDigiHost)
        |  consumed by HGCalCMCalibrationProducer
  [copy to device]
  fill ML input SoA  (HGCalCMCalibKernel_fillInputs)   -- 21 floats per digi
  DNN inference      (TorchScript via AlpakaModel)
  subtract in-place  (HGCalCMCalibKernel_applyCorrections)
  [copy corrected ADCs back to host]
        |  produces
hgcalCMCalibDigis (HGCalDigiHost, CM-corrected)
        |  consumed by HGCalRecHitsProducer (unchanged)
hgcalRecHits -> hgcalRecHitsLayerClusters -> ...
```

---

## DNN Input Features (21 floats, in order)

| Index | SoA column | Description |
|---|---|---|
| 0–11 | `cm0`..`cm11` | Pedestal-subtracted CM sum per eRx (from `digi.cm()` of the first channel of each eRx). Slot is 0 for eRx indices beyond the module's count. |
| 12 | `msubchidx` | Channel index within module, mean-subtracted: `chIdx - (nErx*37 - 1)/2` |
| 13 | `msuberxidx` | eRx index within module, mean-subtracted: `erxIdx - (nErx - 1)/2` |
| 14 | `cellfrac` | Cell area fraction from `HGCalMappingCellParamSoA::trace()` |
| 15–18 | `unconn0`..`unconn3` | Raw ADC of the 4 unconnected channels in this digi's eRx, at within-eRx positions 8, 17, 19, 28 |
| 19 | `ntoa` | Event-level count of digis with TOA > 0 |
| 20 | `ntot` | Event-level count of digis with TOT > 0 |

All 21 columns are `SOA_COLUMN(float, ...)` and declared adjacent in the layout so that
`TensorCollection::add()` can wrap them into a single contiguous `[ndigis, 21]` tensor
without copying.

---

## Model File

- **Path (CMSSW FileInPath):** `HGCalCommissioning/Calibrations/NNBased/cm_dnn.pth`
- **Format:** TorchScript — exported via `torch.jit.script()` + `torch.jit.save()`.
  CMSSW's `cms::torch::load()` requires TorchScript. A plain `torch.save(state_dict())`
  file fails at runtime with `constants.pkl not found`.
- **Architecture:** `Linear(21,256)->ReLU->Linear(256,256)->ReLU->Linear(256,256)->ReLU->Linear(256,32)->ReLU->Linear(32,1)`
- **Source state dict:** `/eos/user/t/ttravis/TestBeam2026/dnn_best.pth`
- **Conversion script:** `/tmp/convert_model.py`

---

## Indexing Architecture

Understanding the index space prevents out-of-bounds bugs:

- `HGCalDenseIndexInfoDevice` has `maxDataSize()` entries, indexed by **global channel ID**.
  Entry `i` describes the module, eRx, and within-module position of global channel `i`.
- For the TB2025 test beam `maxDataSize() = 2220`. All 2220 test beam channels have global
  IDs 0..2219, so `digi_view[idx]` and `index_view[idx]` are a direct 1-to-1 mapping for
  `idx = 0..ndigis-1`.
- `chDataOffsets_[modid]` = first global channel ID for module `modid` = also the first digi
  SoA index for that module (10 modules, 222 channels each, offsets 0, 222, 444, …, 1998).
- `maxModulesCount()` = 10 physical modules. `maxModuleSize()` = 1 (number of distinct
  typecodes — all modules share the same ECON-D type). These are **not** the same; sizing
  arrays by `maxModuleSize()` causes UB when accessing module indices 1–9.
- `enabledErx_[modid]` is a **bitmask** (bit `e` set means eRx `e` is active), not a count.
  For the test beam `enabledErx = 0x3f` (6 eRxs). The count for mean-subtraction is
  `__builtin_popcount(mask)`.

---

## All Files Created or Modified

### NEWLY CREATED

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/HGCALSoACMML.h`

Defines the 21-column SoA layout for the per-channel DNN input features. Uses namespace
`hgcalcmml` to avoid collision with `hgcaldigi::HGCalDigiSoA`. All columns are
`SOA_COLUMN(float, ...)` and declared adjacent so `TensorCollection::add<SoA>(...)` can
wrap them as a single contiguous tensor. `SOA_EIGEN_COLUMN` was deliberately avoided because
the `records()` accessor on a SoA view does not expose Eigen columns.

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/HGCALSoACMMLDeviceCollection.h`

Thin alpaka device collection wrapper:
```cpp
namespace ALPAKA_ACCELERATOR_NAMESPACE {
  using HGCalSoACMMLDeviceCollection = PortableCollection<hgcalcmml::HGCalCMMLSoA>;
}
```

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/HGCalCMCorrectionSoA.h`

Single-column SoA for the DNN output (one float per digi):
```cpp
namespace hgcalcmml {
  GENERATE_SOA_LAYOUT(HGCalCMCorrectionSoALayout, SOA_COLUMN(float, correction))
  using HGCalCMCorrectionSoA = HGCalCMCorrectionSoALayout<>;
}
```

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCMCorrectionDeviceCollection.h`

Device collection wrapper for the correction SoA:
```cpp
namespace ALPAKA_ACCELERATOR_NAMESPACE {
  using HGCalCMCorrectionDeviceCollection = PortableCollection<hgcalcmml::HGCalCMCorrectionSoA>;
}
```

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCMCalibrationAlgorithms.h`

Declares `HGCalCMCalibrationAlgorithms` (inside `ALPAKA_ACCELERATOR_NAMESPACE`) with two
kernel-launching methods and a private `n_threads_` member:

```cpp
void fillCMInputs(Queue&, uint32_t ndigis, int ntoa, int ntot,
                  hgcaldigi::HGCalDigiDevice const&,
                  hgcal::HGCalDenseIndexInfoDevice const&,
                  hgcal::HGCalMappingCellParamDevice const&,
                  uint32_t const* d_chDataOffsets,
                  uint32_t const* d_enabledErx,
                  HGCalSoACMMLDeviceCollection&) const;

void applyCMCorrections(Queue&, uint32_t ndigis,
                        HGCalCMCorrectionDeviceCollection const&,
                        hgcaldigi::HGCalDigiDevice&) const;
```

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalCMCalibrationAlgorithms.dev.cc`

The `.dev.cc` extension causes the build system to compile this for every alpaka backend
(CPU serial, CUDA, ROCm). Contains two kernels and their launcher methods.

**`HGCalCMCalibKernel_fillInputs`** — one thread per digi (indexed 0..ndigis-1):
1. Reads `index_view[idx].modInfoIdx()` (dense module index, 0..9) and `.chNumber()`
   (position within module, 0..221) from `HGCalDenseIndexInfoDevice`.
2. Derives `erxIdx = chIdx / 37`.
3. Looks up `enabledErxMask = d_enabledErx[denseModIdx]` (bitmask) and
   `chOffset = d_chDataOffsets[denseModIdx]` (first digi SoA index for this module).
4. Computes `nErx = __builtin_popcount(enabledErxMask)` for mean-subtracted features.
5. Fills `cm0`..`cm11` via macro `FILL_CM(e_)` using bitmask guard
   `(enabledErxMask >> e_) & 1u`: reads `digi_view[chOffset + e_*37].cm()` if the eRx
   bit is set, else 0. The bitmask guard is essential — the raw mask value (e.g. 63) is
   not a count and cannot be used with `e_ < nErx`.
6. Fills `msubchidx`, `msuberxidx`, `cellfrac` (from `cellmap_view[idx.cellInfoIdx()].trace()`).
7. Fills `unconn0`..`unconn3` from `digi_view[chOffset + erxIdx*37 + {8,17,19,28}].adc()`.
8. Broadcasts event-level `ntoa`, `ntot` to every slot.

**`HGCalCMCalibKernel_applyCorrections`** — one thread per digi:
```cpp
float corrected = float(digi_view[idx].adc()) - corr_view[idx].correction();
digi_view[idx].adc() = uint16_t(std::clamp(corrected, 0.0f, 65535.0f));
```
Correction is **subtractive**: the model predicts the noise contribution and it is removed.

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalCMCalibrationProducer.cc`

A `stream::EDProducer<>` registered via `DEFINE_FWK_ALPAKA_MODULE` (one instance per
backend). Consumes `HGCalDigiHost` from `hgcalDigis` and produces a corrected
`HGCalDigiHost` that downstream modules consume as `hgcalCMCalibDigis`.

**Tokens:**

| Token | Type | Source |
|---|---|---|
| `digisToken_` | `HGCalDigiHost` (get) | `hgcalDigis` |
| `correctedDigisToken_` | `HGCalDigiHost` (put) | default label |
| `moduleIndexerToken_` | `HGCalMappingModuleIndexer` | `hgCalMappingESProducer` |
| `indexingToken_` | `HGCalDenseIndexInfoDevice` | `hgCalDenseIndexInfoESProducer` |
| `cellmapToken_` | `HGCalMappingCellParamDevice` | `hgCalMappingCellESProducer` |

**`produce()` step-by-step:**
1. Build `h_chDataOffsets[]` and `h_enabledErx[]` of size `moduleIndexer.maxModulesCount()`
   on the host from `fedReadoutSequences()`. Size must be `maxModulesCount()` (total physical
   modules, e.g. 10), not `maxModuleSize()` (distinct typecodes, e.g. 1).
2. Upload to device via `make_device_buffer` + `make_host_view` + `alpaka::memcpy`.
3. Copy `hostDigis` to a `HGCalDigiDevice`; compute `ntoa`/`ntot` with a host loop.
4. Allocate `HGCalSoACMMLDeviceCollection(queue, ndigis)` and call `algo_.fillCMInputs()`.
5. Run TorchScript inference via `AlpakaModel::forward()`:
   ```cpp
   const int batchSize = static_cast<int>(ndigis);   // named int avoids most-vexing-parse
   TensorCollection<Queue> inputs{batchSize};
   inputs.template add<hgcalcmml::HGCalCMMLSoA>("features",
       in_records.cm0(), ..., in_records.ntot());     // [ndigis, 21] tensor
   TensorCollection<Queue> outputs{batchSize};
   outputs.template add<hgcalcmml::HGCalCMCorrectionSoA>("correction",
       out_records.correction());
   model_.forward(queue, inputs, outputs);
   ```
6. Call `algo_.applyCMCorrections()` — subtracts prediction from `deviceDigis` ADC in-place.
7. Copy corrected digis back to `HGCalDigiHost` and emplace. The async `alpaka::memcpy` is
   drained by the framework after `produce()` returns; no explicit `alpaka::wait()` needed.

**Key pitfall — `produces()` syntax:** In an alpaka `stream::EDProducer`, host products must
be registered as `produces().produces<HGCalDigiHost>()` (calling `.produces<T>()` on the
`ProducerBaseAdaptor` returned by the no-arg `produces()`). Using `produces<HGCalDigiHost>()`
directly fails because the alpaka base class interprets the template argument as an
`edm::Transition`.

---

#### `HGCalCommissioning/Calibrations/NNBased/cm_dnn.pth`

TorchScript model file loaded at runtime via `cms::torch::load()`. The original file was a
plain `torch.save(model.state_dict())` export which CMSSW cannot load — the loader requires
TorchScript format and fails with `constants.pkl not found` on a state dict.

The model was reconstructed from the state dict key shapes, scripted with `torch.jit.script()`,
and saved with `torch.jit.save()`. Source: `/eos/user/t/ttravis/TestBeam2026/dnn_best.pth`.
Conversion script: `/tmp/convert_model.py`.

---

### MODIFIED

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/BuildFile.xml`

Added dependencies required by the CM calibration producer and kernels:

| Dependency | Reason |
|---|---|
| `CondFormats/DataRecord` | ES record types for `HGCalElectronicsMappingRcd`, `HGCalDenseIndexInfoRcd` |
| `CondFormats/HGCalObjects` | `HGCalMappingModuleIndexer`, mapping parameter types |
| `PhysicsTools/PyTorchAlpaka` | `AlpakaModel`, `TensorCollection` |
| `pytorch-cuda` (for alpaka/cuda) | Links `c10::cuda` symbols from `AlpakaModel::forward`; without this the CUDA build links but fails at runtime with `undefined reference to c10::cuda::getCurrentCUDAStream` |

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/HGCALSoACMML.h`
*(created, then later modified)*

Initially 18 columns. Expanded to **21 columns** after the model input count was confirmed
with model author Arne Reimers: the single `msubunconnectedch` (sum of 4 unconnected-channel
ADCs) was replaced by four individual `unconn0`..`unconn3` columns, matching the training
data order.

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalCMCalibrationAlgorithms.h`
*(created, then later modified)*

Parameter name `nchannels` renamed to `ndigis` in both `fillCMInputs` and
`applyCMCorrections` declarations, to make clear the loop bound is the per-event digi count
(e.g. 2220), not the total detector channel count (e.g. 32580).

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalCMCalibrationAlgorithms.dev.cc`
*(created, then later modified)*

Four corrections applied after the initial implementation:

1. **Wrong iteration bound** — kernel was iterating `maxDataSize()` (32580) but `digi_view`
   and `index_view` only have `ndigis` (2220) entries. Fixed to iterate `ndigis`.

2. **Wrong eRx guard in FILL_CM** — `enabledErx` is a bitmask, not a count. Guard was
   `e_ < nErx` which treated the bitmask value (63) as a number, allowing all 12 iterations
   to fire and producing accesses like `digi_view[1998 + 11*37] = digi_view[2405]`, out of
   range 2220. Fixed to `(enabledErxMask >> e_) & 1u`.

3. **Four individual `unconn` columns** — filling `unconn0`..`unconn3` individually to match
   the expanded SoA layout.

4. **Subtractive correction** — changed `adc + correction` to `adc - correction`.

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalCMCalibrationProducer.cc`
*(created, then later modified)*

Three corrections applied after the initial implementation:

1. **Wrong module array size** — `nmodules = moduleIndexer.maxModuleSize()` returned 1
   (number of distinct typecodes) instead of 10 (physical modules). Module indices run 0..9;
   size-1 device arrays caused undetected out-of-bounds access for all but module 0. Fixed to
   `moduleIndexer.maxModulesCount()`.

2. **Wrong SoA allocation size** — `deviceMLSoA` and `deviceCorrections` were allocated with
   `nchannels` (= `maxDataSize()` = 2220 for the test beam, but read from the wrong variable).
   Changed to allocate with `ndigis` (= actual per-event digi count from the host collection).

3. **Updated `inputs.add()`** — now passes all 21 SoA columns, replacing `msubunconnectedch`
   with `unconn0`, `unconn1`, `unconn2`, `unconn3`.

---

#### `HGCalCommissioning/Configuration/python/configure_sysval_reco_cff.py`

Two changes:

1. **Inserted `hgcalCMCalibDigis` in the RECO task** — `HGCalCMCalibrationProducer` is
   added between the raw-to-digi unpacker and `HGCalRecHitsProducer` in both the GPU
   (`alpaka_cuda_async`) and serial (`alpaka_serial_sync`) config paths. The `digis` input
   tag on `hgcalRecHitsProducer` is redirected from `hgcalDigis` to `hgcalCMCalibDigis`.
   `hgcalCMCalibDigis` is also added to the `reco_task` in all three task variants
   (`skipLC=True`, `withLC`, `withTracksters`).

2. **Fixed `cellmapSource` ES label** — changed from `hgCalMappingModuleESProducer` (which
   produces module-level parameters) to `hgCalMappingCellESProducer` (which produces
   `HGCalMappingCellParamHost`, needed for `cellfrac`). The wrong label caused a
   `NoProductResolverException` at startup.

---

#### `HGCalCommissioning/DQM/plugins/HGCalSysValDigisClient.cc`

No code changes. The compiled object file was 544 bytes (stale/corrupt from a previous
partial build), so the `DEFINE_FWK_MODULE(HGCalSysValDigisClient)` registration symbol was
never linked into the `.so`. Deleting the stale `.o` and running `scram b` produced a
3.27 MB object with the plugin properly registered. Confirmed via `edmPluginDump`.

---

## Running

```bash
cmsRun $CMSSW_BASE/src/HGCalCommissioning/Configuration/test/step_RECONANODQM.py \
  era=TB2025/v6 run=112048 \
  files=$(paste -sd, fileList.txt) \
  maxEvents=10000 knoise=-1 \
  elossfile=HGCalCommissioning/Calibrations/TB2025/calib/hgcal_energyloss_setup2_v1.json \
  calibfile=HGCalCommissioning/Calibrations/TB2025/calib/level0_calib_params_v5.json \
  skipLC=True
```

Tested on run 112048, 10,000 events, ~177 ev/s on serial backend. No exceptions.
DQM output: `DQM_V0001_HGCAL_R000112048.root`.

---

## Known Limitations / Future Work

- **Pedestal subtraction for unconnected channels:** `unconn0`..`unconn3` store raw ADC.
  The model was trained on pedestal-subtracted values; a pedestal lookup for positions
  8, 17, 19, 28 per eRx should be applied before filling these features.

- **Unconnected channel positions hardcoded:** `{8, 17, 19, 28}` is correct for LD silicon
  modules. SiPM modules or future layouts may differ.

- **37 channels per eRx hardcoded:** Valid for LD modules; needs parameterization for mixed
  geometries.

- **Per-event module array upload:** `h_chDataOffsets` and `h_enabledErx` are rebuilt and
  uploaded on every event. These are conditions-constant per run and could be moved into an
  ESProducer for better throughput.
