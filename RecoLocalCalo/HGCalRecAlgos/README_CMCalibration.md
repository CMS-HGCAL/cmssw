# HGCal Common-Mode Calibration via PyTorch DNN (alpaka)

This document covers every file created or modified to implement the common-mode (CM) noise
correction for HGCal digis using a PyTorch DNN running on device (GPU or CPU) via the CMSSW
alpaka portability layer. C++ lives in `RecoLocalCalo/HGCalRecAlgos`; Python wiring and the
model file live in `HGCalCommissioning/`.

---

## Overview

HGCal HGCROC ASICs read out a common-mode (CM) voltage per eRx that correlates with pedestal
noise on ordinary channels. This implementation runs a per-channel DNN correction that takes
pedestal-subtracted CM sums, channel geometry, pedestal-subtracted unconnected-channel ADCs,
and per-module timing occupancy as input, then **subtracts** the predicted noise from the raw
digi ADC before the RecHit step.

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

**Digi collection naming:** `HGCalCMCalibrationProducer` does **not** overwrite or rename the
original digis. Both collections coexist in the event:

| Collection label | Contents | Producer |
|---|---|---|
| `hgcalDigis:""` | Raw unpacked digis — unchanged | upstream unpacker |
| `hgcalCMCalibDigis:""` | CM-corrected copy | `HGCalCMCalibrationProducer` |

The module label (`hgcalCMCalibDigis`) comes from the Python config; the instance label is
empty (no argument to `produces()`). Downstream modules (`hgcalRecHits`) are wired to one or
the other at config time via `useCMML`; there is no `_old` suffix or renaming of the originals.

---

## DNN Input Features (21 floats, in order)

| Index | SoA column | Description |
|---|---|---|
| 0–11 | `cm0`..`cm11` | Full-scale pedestal-subtracted CM per eRx: `digi.cm() - 2*CM_ped` (= `2*(0.5*digi.cm() - CM_ped)`). `digi.cm()` is the hardware sum of 2 CM channels for eRx `e`, read from the first channel of that eRx (`chOffset + e*37`). `CM_ped` is the half-scale per-channel pedestal from `HGCalCalibParamHost`, doubled here to match the training scale (twice the analytic RecHit term — see the CM channels section). Inactive **and dead/uncalibrated** eRx slots are 0 — see [Calibration validity guard](#calibration-validity-guard). |
| 12 | `msubchidx` | Channel index, mean-subtracted over the whole module: `chIdx - (nErx*37 - 1)/2` (±110.5 on a 6-eRx LD module). See [msubchidx convention](#msubchidx-convention). |
| 13 | `msuberxidx` | eRx index within module, mean-subtracted: `erxIdx - (nErx - 1)/2` |
| 14 | `cellfrac` | Cell area fraction (SF) from `cellareas.json`, indexed by `chIdx`. `isHD()` from `HGCalMappingCellParamSoA` selects the `MH_F` (444-entry) or `ML_F` (222-entry) table. |
| 15–18 | `unconn0`..`unconn3` | Pedestal-subtracted ADC of the 4 unconnected channels in this digi's eRx: `digi.adc() - ADC_ped`. Within-eRx positions 8, 17, 19, 28. `ADC_ped` from `HGCalCalibParamHost` at each channel's global index. Dead/uncalibrated channels are 0 — see [Calibration validity guard](#calibration-validity-guard). |
| 19 | `ntoa` | Per-module count of digis with TOA > 0 (`flags != NotAvailable && toa > 0`), broadcast to every channel of that module |
| 20 | `ntot` | Per-module count of genuine TOT-mode digis (`flags != NotAvailable && tctp == 3 && tot > 0`), broadcast to every channel of that module |

All 21 columns are `SOA_COLUMN(float, ...)` and declared adjacent in the layout so that
`TensorCollection::add()` can wrap them into a single contiguous `[ndigis, 21]` tensor
without copying.

---

## Pedestal Subtraction

The hardware digi fields `cm()` and `adc()` are **raw** — no pedestal subtraction is done by
the ECON-D unpacker. The kernel applies the same conventions used by the analytic RecHit
calibration path (`HGCalRecHitCalibrationAlgorithms.dev.cc`).

### CM channels (`cm0`..`cm11`)

The HGCROC returns a sum of 2 dedicated CM channels per eRx as `digi.cm()`. The DNN input is
the **full-scale** pedestal-subtracted common mode:

```
cm_e = float(digi_view[chOffset + e*37].cm()) - 2 * CM_ped[chOffset + e*37]
     = 2 * (0.5 * digi.cm() - CM_ped)
```

- `chOffset + e*37` is the global index of the **first channel** in eRx `e` — this is where
  the CM value is stored in the digi SoA.
- `CM_ped` is the per-channel pedestal from `HGCalCalibParamHost` (field `.CM_ped()`),
  the same calibration object consumed by `HGCalRecHitsProducer`. `CM_ped` is the pedestal of
  the **half-scale** CM (`0.5*cm`), so it is doubled here to match the full-scale sum.
- **This is intentionally twice the analytic RecHit term.** The analytic denoising uses the
  half-scale `cmf = CM_slope * (0.5 * float(cm) - CM_ped)`, but the DNN (Arne Reimers) was
  trained on the full-scale `cm - 2*CM_ped`. An earlier version used the half-scale `0.5*cm -
  CM_ped` here, which made every `cm*` feature a factor of 2 too small vs training; the `2*`
  restores the training scale. See the `FILL_CM` macro in
  `HGCalCMCalibrationAlgorithms.dev.cc`.
- Inactive eRx slots (bit not set in `enabledErxMask`) are set to `0.0f` without any lookup.

### Unconnected channels (`unconn0`..`unconn3`)

```
unconn_j = float(digi_view[chOffset + erxIdx*37 + kUnconn[j]].adc()) - ADC_ped[chOffset + erxIdx*37 + kUnconn[j]]
```

- `kUnconn = {8, 17, 19, 28}` — fixed within-eRx positions of the 4 unconnected channels
  on LD silicon modules.
- `ADC_ped` is from `HGCalCalibParamHost` (field `.ADC_ped()`), indexed by the exact global
  channel index of each unconnected channel.

### Calibration validity guard

`HGCalCalibParamSoA` carries a per-channel `valid` flag (`SOA_COLUMN(unsigned char, valid)`,
documented as *"if false = mask dead channel"*). Channels with `valid == 0` have **no fitted
pedestal**: both `ADC_ped` and `CM_ped` are exactly `0.0` in the calibration JSON.

Both the `cm*` and `unconn*` features are gated on this flag. When `valid == 0` the feature is
set to `0.0f` — the same "this eRx contributes nothing" encoding already used for inactive eRx
slots, and therefore a value the DNN saw in training (`cm6`..`cm11` are identically zero on
every 6-eRx LD module).

**Why this matters.** Without the guard, `CM_ped == 0` means the pedestal subtraction is a
no-op and the *raw* CM sum passes straight through. Measured on run 112049: module
`ML-F3WC-IH0199` has a dead eRx 5 (`Valid=0`, `CM_ped=0` in `level0_calib_params_v4/v5.json`),
which produced `cm5 ≈ +529` against a normal range of about `-1.5` for that module. Because
`cm0`..`cm11` are **module-level** features, that single dead eRx poisoned the input vector for
*every channel of the module* — 10 % of all digis in the run — and a permutation-importance
scan attributed 21.8 % of the model's output variance to that one corrupted feature.

The RecHit path has always guarded on this flag (`bool calibvalid = calib.valid();` in
`HGCalRecHitCalibrationKernel_adcToEnergy`); the ML path simply never inherited it.

Verified on run 112050: `cm5 → 0.000` for `ML-F3WC-IH0199` and unchanged (`-3.9` … `+1.3`) for
the other nine modules.

The correction *application* (`applyCMCorrections`) is deliberately **not** gated on `valid` —
it still skips only unconnected channels (`cellfrac == 0`). Digis on a dead eRx are flagged
`NotAvailable` and are dropped by the RecHit kernel's own validity check, so a second guard
there would be dead code.

### msubchidx convention

`msubchidx` is the channel index mean-subtracted over the **whole module**:
`chIdx - (nErx*37 - 1)/2`, i.e. ±110.5 on a 6-eRx LD module.

**This was confirmed (2026-08-24) as the training convention** by the `nchadc = 222` field in
the model author's per-module constants record: 222 is the whole-module channel count, not the
per-eRx 37. A per-eRx alternative `(chIdx % 37) - 18` was implemented and compared while the
convention was unknown; it is not what training used and has been removed.

Note the scale: ±110.5 with std ~64, which dwarfs every other real feature (all O(10)). The
network is fed raw, unstandardized inputs, so that asymmetry is real and intended.

### Normal signal channels

Ordinary (non-CM, non-unconnected) channel ADC pedestal subtraction happens **after** the CM
calibration, in `HGCalRecHitCalibrationAlgorithms.dev.cc`. The full denoising formula is:

```
cmf          = CM_slope × (0.5 × digi.cm() − CM_ped)     # 0 when skipAnalyticCM=True
denoised_adc = (digi.adc() − ADC_ped) − cmf
               − BXm1_slope × (digi.adcm1() − ADC_ped − cmf)
```

- `digi.adc()` is raw; `ADC_ped` subtracts the per-channel hardware offset.
- `cmf` is the linear common-mode correction (suppressed to 0 when the ML path is active,
  since the DNN correction has already been applied to `digi.adc()` before this step).
- The BXm1 term removes residual contribution from the previous bunch crossing using the
  raw ADC of that BX (`digi.adcm1()`), which has not been ML-corrected.
- All three constants (`ADC_ped`, `CM_slope`, `CM_ped`, `BXm1_slope`) come from
  `HGCalCalibParamHost` indexed by global channel index.

So in the ML path, the sequence for a signal channel is:
1. **CM DNN** subtracts the predicted noise from `digi.adc()` in-place.
2. **RecHit kernel** subtracts `ADC_ped` and the BXm1 term from the (already CM-corrected) ADC.

### Calibration source

`HGCalCMCalibrationProducer` now consumes `HGCalCalibParamHost` via:
```cpp
edm::ESGetToken<hgcalrechit::HGCalCalibParamHost, HGCalModuleConfigurationRcd> calibToken_;
```
configured as `calibSource=cms.ESInputTag('hgcalCalibParamESProducer', '')` in the Python
config. At the start of each event the `ADC_ped` and `CM_ped` columns are extracted into flat
host vectors and uploaded to device buffers, which are then passed to the kernel as
`float const* d_adcPed` and `float const* d_cmPed`.

### CM pedestal override

The DNN author (Arne Reimers) records the CM pedestals the model was **trained** against in a
different convention from the calibration file: one **full-scale** value per eRx, keyed by
module typecode, in a per-module record that also carries the module geometry:

```json
{"ML-F3WC-IH0182": {"cm_erx00": 194.32644052044608, ..., "cm_erx11": 0.0,
                    "nchadc": 222.0, "nerx": 6.0}}
```

`cm_erxNN = 2 * CM_ped`, so it is the *same quantity* as the calib file's `CM_ped` at twice the
scale — his feature is `cm_raw - cm_erxNN`, ours is `2*(0.5*cm - CM_ped)`, which is the same
quantity. The `cmPedOverride` parameter loads such a file and replaces `CM_ped` for the
listed modules, halving each value on load so the kernel is untouched. `ADC_ped` is never
overridden (the record carries none), the `valid()` guard still applies, and modules absent
from the file keep their calib-file pedestals.

`nchadc`/`nerx` are cross-checked against the mapping and the job **throws** on mismatch.
`nchadc` is the whole-module channel count (222 on a 6-eRx LD module, not the per-eRx 37) —
the same quantity that fixes the [msubchidx convention](#msubchidx-convention), which is how
that convention was confirmed to be the per-module one.

Measured on `ML-F3WC-IH0182`, run 112049, 1000 events (`evaluate_arne_pedestals.py`): his
pedestals sit a constant **0.32–0.50 counts** below `2*CM_ped(v5)`, i.e. 3.6–7.3% of each `cm`
feature's RMS, but the resulting DNN correction moves by only **0.036 ± 0.090 ADC** (mean
|Δ| 0.070, 1.7% of the correction). The pedestal source is *not* what drives the correction.

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
                  float const* d_sfLD, float const* d_sfHD,
                  float const* d_adcPed, float const* d_cmPed,
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
   `(enabledErxMask >> e_) & 1u`: computes the full-scale `digi.cm() - 2*d_cmPed[chOffset+e_*37]`
   (= `2*(0.5*cm - CM_ped)`, the training scale) if the eRx bit is set, else 0. The bitmask
   guard is essential — the raw mask value (e.g. 63) is not a count and cannot be used with
   `e_ < nErx`.
6. Fills `msubchidx`, `msuberxidx`, `cellfrac` (SF from `cellareas.json` via `d_sfLD`/`d_sfHD`,
   selected by `cellmap_view[idx.cellInfoIdx()].isHD()`).
7. Fills `unconn0`..`unconn3` as `digi.adc() - d_adcPed[...]` at within-eRx positions 8, 17, 19, 28.
8. Broadcasts per-module `ntoa`, `ntot` to every slot via `d_ntoa[denseModIdx]` /
   `d_ntot[denseModIdx]` (computed host-side; see producer step 3).

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
| `calibToken_` | `HGCalCalibParamHost` | `hgcalCalibParamESProducer` |

**`produce()` step-by-step:**
1. Build `h_chDataOffsets[]` and `h_enabledErx[]` of size `moduleIndexer.maxModulesCount()`
   on the host from `fedReadoutSequences()`. Size must be `maxModulesCount()` (total physical
   modules, e.g. 10), not `maxModuleSize()` (distinct typecodes, e.g. 1).
2. Upload to device via `make_device_buffer` + `make_host_view` + `alpaka::memcpy`.
3. Copy `hostDigis` to a `HGCalDigiDevice`; compute **per-module** `ntoa`/`ntot` with a host
   loop (each module's channels are contiguous: `chDataOffsets[m]` .. `+ popcount(enabledErx[m])*37`),
   then upload the two `[nmodules]` arrays to device (`d_ntoa`/`d_ntot`).
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

Corrections and additions applied after the initial implementation:

1. **Wrong iteration bound** — kernel was iterating `maxDataSize()` (32580) but `digi_view`
   and `index_view` only have `ndigis` (2220) entries. Fixed to iterate `ndigis`.

2. **Wrong eRx guard in FILL_CM** — `enabledErx` is a bitmask, not a count. Guard was
   `e_ < nErx` which treated the bitmask value (63) as a number, allowing all 12 iterations
   to fire and producing accesses like `digi_view[1998 + 11*37] = digi_view[2405]`, out of
   range 2220. Fixed to `(enabledErxMask >> e_) & 1u`.

3. **Four individual `unconn` columns** — filling `unconn0`..`unconn3` individually to match
   the expanded SoA layout.

4. **Subtractive correction** — changed `adc + correction` to `adc - correction`.

5. **`cellfrac` from `cellareas.json`** — was incorrectly using `cellmap_view[...].trace()`.
   Fixed to use per-channel SF values loaded from `cellareas.json` at startup, selected by
   `cellmap_view[...].isHD()` to choose the `ML_F` (222-entry) or `MH_F` (444-entry) table.
   Two new pointer parameters: `float const* d_sfLD`, `float const* d_sfHD`.

6. **Pedestal subtraction for DNN inputs** — `cm0`..`cm11` and `unconn0`..`unconn3` were
   raw hardware values. Now:
   - CM inputs: `0.5f * digi.cm() - d_cmPed[chOffset + e*37]`
   - Unconnected inputs: `digi.adc() - d_adcPed[chOffset + erxIdx*37 + kUnconn[j]]`
   Two new pointer parameters: `float const* d_adcPed`, `float const* d_cmPed`.

7. **Debug output moved out of the kernel** — the kernel no longer prints or takes any debug
   parameters. The event/module/channel filter and the dump of the 21 DNN features now live
   host-side in the producer (`writeDebugCsv`, CSV output). See "Debugging DNN Inputs" section.

8. **Calibration validity guard** — `FILL_CM(e)` and the new `FILL_UNCONN(j)` macro now
   require `d_valid[...] != 0` in addition to the enabled-eRx bit, emitting `0.0f` otherwise.
   New pointer parameter `unsigned char const* d_valid`. Prevents a dead eRx
   (`valid==0` ⇒ `CM_ped==0`) from leaking a raw, unsubtracted CM into a module-level feature.
   See [Calibration validity guard](#calibration-validity-guard).

9. **Feature order fixed (2026-08-24)** — the `SOA_COLUMN` order now matches the model's
   training order: `cm0..cm11, ntoa, ntot, msubchidx, msuberxidx, cellfrac, unconn0..3`.
   The previous layout put `ntoa`/`ntot` last and scored **-238 %** variance explained against
   the author's own training targets, versus **+24.2 %** for this one.

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalCMCalibrationProducer.cc`
*(created, then later modified)*

Corrections and additions applied after the initial implementation:

1. **Wrong module array size** — `nmodules = moduleIndexer.maxModuleSize()` returned 1
   (number of distinct typecodes) instead of 10 (physical modules). Module indices run 0..9;
   size-1 device arrays caused undetected out-of-bounds access for all but module 0. Fixed to
   `moduleIndexer.maxModulesCount()`.

2. **Wrong SoA allocation size** — `deviceMLSoA` and `deviceCorrections` were allocated with
   `nchannels` (= `maxDataSize()` = 2220 for the test beam, but read from the wrong variable).
   Changed to allocate with `ndigis` (= actual per-event digi count from the host collection).

3. **Updated `inputs.add()`** — now passes all 21 SoA columns, replacing `msubunconnectedch`
   with `unconn0`, `unconn1`, `unconn2`, `unconn3`.

4. **`cellareas.json` loading** — constructor now reads `ML_F.SF` (222 entries) and
   `MH_F.SF` (444 entries) from a `FileInPath`-resolved JSON file into `h_sfLD_` and
   `h_sfHD_`. These are uploaded as flat float device buffers each event and passed to the
   kernel.

5. **Pedestal subtraction for DNN inputs** — added `calibToken_` consuming
   `HGCalCalibParamHost` from `hgcalCalibParamESProducer`. Each event, `ADC_ped` and `CM_ped`
   columns are extracted into host vectors and uploaded to device. Passed to `fillCMInputs` as
   `d_adcPed` / `d_cmPed`. The `valid` column is extracted and uploaded alongside them as
   `d_valid` (see [Calibration validity guard](#calibration-validity-guard)).

6. **Debug parameters** — `uint` config parameters: `debugEvent` (0=off,
   UINT32_MAX=any event), `debugModule` (UINT32_MAX=any), `debugMaxChannels` (UINT32_MAX=all),
   `debugFedId` + `debugCaptureBlock` + `debugEcond` (hardware address form of module
   selection, resolved host-side via `moduleIndexer.getIndexForModule()`), plus a string
   `debugCsv` (empty=off) giving the output CSV path. When `debugCsv` is set and the event
   passes `debugEvent`, the producer copies the filled ML-input SoA back to host and, in
   `writeDebugCsv`, appends one row per (event, module, channel) passing the filter. Columns:
   `event, denseModIdx, module` (typecode, e.g. `ML-F3WC-IH0198`, from `typecodeMap()`),
   `ch, digi_adc, unconn0_raw`, then the 21 SoA inputs. A `std::mutex` serialises appends
   across stream instances and a `std::once_flag` writes the header once (truncating any file
   from a prior run).

7. **Missing `HGCalRawDataDefinitions.h`** — `hgcal::DIGI_FLAG::NotAvailable` requires this
   header; it is not transitively included by `HGCalDigiHost.h`. Added explicit include.

---

#### `HGCalCommissioning/Configuration/python/configure_sysval_reco_cff.py`

Three changes:

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

3. **Added `useCMML` command-line toggle** — a `VarParsing` bool (`True` by default) that
   switches between ML and analytic correction modes. When `False`: `hgcalCMCalibDigis` is
   not created, `hgcalRecHits` reads `hgcalDigis` directly, and `skipAnalyticCM=False` is
   passed so the analytic correction runs. When `True` (default): ML producer is inserted
   and `skipAnalyticCM=True` suppresses the analytic term. See "Switching Between ML and
   Analytic CM Correction" section.

---

#### `RecoLocalCalo/HGCalRecAlgos/interface/alpaka/HGCalRecHitCalibrationAlgorithms.h`
*(modified)*

Added `bool skipAnalyticCM = false` as a trailing parameter to `calibrate()`.

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalRecHitCalibrationAlgorithms.dev.cc`
*(modified)*

Added `bool skipAnalyticCM_` member to `HGCalRecHitCalibrationKernel_adcToEnergy`. In the
`adc_denoise` lambda the CM term is now:
```cpp
float cmf = skipAnalyticCM_ ? 0.f : cm_slope * (0.5f * float(cm) - cm_ped);
```
Updated `calibrate()` definition and the kernel launch (`HGCalRecHitCalibrationKernel_adcToEnergy{skipAnalyticCM}`)
to thread the flag through.

---

#### `RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalRecHitProducers.cc`
*(modified)*

Added `bool skipAnalyticCM_` member, read from `iConfig.getParameter<bool>("skipAnalyticCM")`,
registered in `fillDescriptions` with default `false`. Forwarded to `calibrator_.calibrate()`.

---

#### `HGCalCommissioning/Configuration/test/step_RECONANODQM.py`

*(modified)*

Registered the `cmPedOverride` and `nThreads` VarParsing options and forwarded them to
`process.hgcalCMCalibDigis` / `process.options`.

Not compiled — lives in `test/` and is read directly by `cmsRun`, so edits need no `scram b`.

---

#### `HGCalCommissioning/Configuration/test/run_cm_comparison.sh`

*(modified)*

Runs the RECO+DQM step twice (ML and analytic) on the same input, writing DQM files to
`<outdir>/ml/` and `<outdir>/analytic/`. Not compiled.

---

#### `HGCalCommissioning/DQM/plugins/HGCalSysValDigisClient.cc`

No code changes. The compiled object file was 544 bytes (stale/corrupt from a previous
partial build), so the `DEFINE_FWK_MODULE(HGCalSysValDigisClient)` registration symbol was
never linked into the `.so`. Deleting the stale `.o` and running `scram b` produced a
3.27 MB object with the plugin properly registered. Confirmed via `edmPluginDump`.

---

## Switching Between ML and Analytic CM Correction

The RECO step accepts a `useCMML` boolean argument (default `True`) to select the correction mode.

**ML correction (default):**
```bash
cmsRun $CMSSW_BASE/src/HGCalCommissioning/Configuration/test/step_RECONANODQM.py \
  era=TB2025/v6 run=112048 \
  files=$(paste -sd, fileList.txt) \
  maxEvents=10000 knoise=-1 \
  elossfile=HGCalCommissioning/Calibrations/TB2025/calib/hgcal_energyloss_setup2_v1.json \
  calibfile=HGCalCommissioning/Calibrations/TB2025/calib/level0_calib_params_v5.json \
  skipLC=True
# useCMML=True is the default
```

**Analytic CM correction only (old behaviour):**
```bash
cmsRun $CMSSW_BASE/src/HGCalCommissioning/Configuration/test/step_RECONANODQM.py \
  era=TB2025/v6 run=112048 \
  files=$(paste -sd, fileList.txt) \
  maxEvents=10000 knoise=-1 \
  elossfile=HGCalCommissioning/Calibrations/TB2025/calib/hgcal_energyloss_setup2_v1.json \
  calibfile=HGCalCommissioning/Calibrations/TB2025/calib/level0_calib_params_v5.json \
  skipLC=True useCMML=False
```

When `useCMML=True`:
- `hgcalCMCalibDigis` (ML DNN producer) is inserted before `hgcalRecHits`
- `hgcalRecHits` reads from `hgcalCMCalibDigis` instead of `hgcalDigis`
- `skipAnalyticCM=True` is passed to `HGCalRecHitsProducer`, which sets `cmf = 0` in
  `HGCalRecHitCalibrationKernel_adcToEnergy`, suppressing the analytic
  `cm_slope * (0.5*cm - cm_ped)` subtraction

When `useCMML=False`:
- `hgcalRecHits` reads directly from `hgcalDigis`
- The analytic CM correction runs normally via `CM_slope` / `CM_ped` from the calibration file

The two modes are mutually exclusive: the ML correction and the analytic correction do **not** compound.

Tested on run 112048, 10,000 events, ~177 ev/s on serial backend. No exceptions.
DQM output: `DQM_V0001_HGCAL_R000112048.root`.

### CM-related `step_RECONANODQM.py` arguments

All are `cmsRun` keyword arguments (VarParsing). Everything below applies only when
`useCMML=True`; the debug group additionally requires `debugEvent != 0`.

| Argument | Default | Meaning |
|---|---|---|
| `useCMML` | `True` | Insert `hgcalCMCalibDigis` (ML DNN) before `hgcalRecHits` and set `skipAnalyticCM=True`. `False` = analytic CM only. |
| `addCMComparison` | `False` | Also schedule the `HGCalCMCorrectionCompare` analyzer. Requires `useCMML=True`. |
| `cmPedOverride` | `''` | Path to a JSON of per-module CM pedestals in the DNN author's convention (`{typecode: {cm_erx00..cm_erx11, nchadc, nerx}}`, `cm_erxNN` full-scale `= 2*CM_ped`). Replaces `CM_ped` from the calib file for the listed modules only; `ADC_ped` is never touched. Empty = use the calib file. See [CM pedestal override](#cm-pedestal-override). |
| `trigTimeMin` / `trigTimeMax` | `0` / `0` | Keep events with `min < trig_time < max` (exclusive). Active only when `max > min`. |
| `trigTimeRequireValid` | `True` | Require the trig-time e-link `valid_` bit when selecting. |
| `debugEvent` | `0` | Event number to dump DNN inputs for. `0` = off, `4294967295` = every event. **Gates the whole debug group.** |
| `debugCsv` | `''` | Path to write the 21 inputs + IDs as CSV. Empty = off. Requires `debugEvent != 0`. |
| `debugModule` | `4294967295` | Dense module index filter; `4294967295` = all modules. |
| `debugMaxChannels` | `4294967295` | Max within-module `chIdx` to dump. Set to e.g. `2` to keep the CSV small. |
| `debugFedId` / `debugCaptureBlock` / `debugEcond` | `4294967295` | Select a module by hardware address instead of dense index. All three must be set to take effect; they then override `debugModule`. |

> **CSV size warning:** `debugEvent=4294967295` with no `debugMaxChannels` limit writes every
> feature of every digi of every event. On run 112049 that produced a **50 GB** file. Always
> bound it with `debugMaxChannels` and/or a single `debugEvent` unless you genuinely want the
> full dump.

Minimal example — confirm the validity guard and the per-eRx mean on 5 events:

```bash
cd $CMSSW_BASE/src/<some scratch dir>
cmsRun $CMSSW_BASE/src/HGCalCommissioning/Configuration/test/step_RECONANODQM.py \
  era=TB2025/v6 run=112050 \
  files=$(paste -sd, $CMSSW_BASE/src/HGCalCommissioning/Configuration/test/fileList_112050.txt) \
  maxEvents=5 knoise=-1 skipLC=True skipMeta=True \
  elossfile=HGCalCommissioning/Calibrations/TB2025/calib/hgcal_energyloss_setup2_v1.json \
  calibfile=HGCalCommissioning/Calibrations/TB2025/calib/level0_calib_params_v5.json \
  useCMML=True \
  debugEvent=1 debugMaxChannels=2 debugCsv=verify_cm5.csv
```

Note `files=` takes a **comma-separated list of filenames**, not a path to a list file —
expand it with `paste -sd,` as above. (`run_cm_comparison.sh` does this for you.)

### Comparing the Two Modes

A dedicated script and DQM analyzer exist to compare the ML and analytic corrections on the
same input data without any manual file shuffling.

#### `run_cm_comparison.sh`

**Location:** `HGCalCommissioning/Configuration/test/run_cm_comparison.sh`

Runs `step_RECONANODQM.py` twice on the same input — once with `useCMML=True` (ML + the
`HGCalCMCorrectionCompare` analyzer) and once with `useCMML=False` (analytic only) — and
collects the results into a timestamped output directory.

```bash
cd $CMSSW_BASE/src
./HGCalCommissioning/Configuration/test/run_cm_comparison.sh \
    --era TB2025/v6 \
    --run 112048 \
    --files fileList.txt \
    --maxevents 10000 \
    --eloss HGCalCommissioning/Calibrations/TB2025/calib/hgcal_energyloss_setup2_v1.json \
    --calib HGCalCommissioning/Calibrations/TB2025/calib/level0_calib_params_v5.json
```

`--files` accepts either a comma-separated string or a path to a file with one filename per
line. If omitted, the script looks for `fileList.txt` next to itself.

The script runs `scram b -j 8` before the two cmsRun calls by default. Pass `--nobuild` to
skip the build if the code is already compiled. `step_RECONANODQM.py` and the script itself
live in `test/` and are **not** compiled, so edits to either take effect without a rebuild;
only C++ changes under `RecoLocalCalo/` need `scram b`.

**Full flag list:**

| Flag | Default | Meaning |
|---|---|---|
| `--era` | `TB2025/v6` | Era string passed as `era=`. |
| `--run` | `112048` | Run number. |
| `--files` | `fileList.txt` beside the script | Comma-separated list, or a path to a one-per-line file (expanded with `paste -sd,`). |
| `--maxevents` | `10000` | `maxEvents=`. |
| `--knoise` | `-1` | `knoise=`. |
| `--eloss` | `TB2025/calib/hgcal_energyloss_setup2_v1.json` | Energy-loss JSON. |
| `--calib` | `TB2025/calib/level0_calib_params_v5.json` | Level-0 calibration JSON. |
| `--outdir` | `cm_comparison_<timestamp>` | Output directory. |
| `--jobs` | `8` | `scram b -j N`. |
| `--nobuild` | off | Skip `scram b`. |
| `--skipmeta` | off | Pass `skipMeta=True`. **Required for 2025 data** — see the release `CLAUDE.md` for the ROOT scalar→vector streamer crash it avoids. |
| `--trigtimemin N` / `--trigtimemax N` | `0` / `0` | Trig-phase window, exclusive. Active only when max > min. |

Both cmsRun jobs (ML and analytic) receive the same `COMMON_ARGS`, so any flag above applies
consistently to the pair.

Example — per-eRx mean subtraction, raw (un-standardized) inputs, trig-phase window:

```bash
cd $CMSSW_BASE/src
./HGCalCommissioning/Configuration/test/run_cm_comparison.sh \
    --era TB2025/v6 --run 112050 \
    --files HGCalCommissioning/Configuration/test/fileList_112050.txt \
    --maxevents 10000 --nobuild --skipmeta \
    --pererxmeansub \
    --trigtimemin 107 --trigtimemax 113 \
    --outdir cm_comparison_newdnn_pererx_trig107to113_112050
```

**Output directory layout** (name defaults to `cm_comparison_<timestamp>/`):

```
cm_comparison_20260619_191020/
  ml/
    DQM_V0001_HGCAL_R000112048.root   # DQM with ML rechits + CMComparison plots
    NANO_numEvent10000.root
    cmsRun_ml.log
  analytic/
    DQM_V0001_HGCAL_R000112048.root   # DQM with analytic rechits
    NANO_numEvent10000.root
    cmsRun_analytic.log
```

An example output from run 112048, 10 000 events lives under the working release at
`/eos/user/t/ttravis/TestBeam2026/CMSSW_16_1_0/src/` (e.g. a `cm_comparison_<timestamp>/`
directory produced by `run_cm_comparison.sh`).

#### `HGCalCMCorrectionCompare` analyzer

**Plugin:** `HGCalCommissioning/DQM/plugins/HGCalCMCorrectionCompare.cc`  
**cfi:** `HGCalCommissioning/DQM/python/hgcalCMCorrectionCompare_cfi.py`  
**Wired by:** `step_RECONANODQM.py` when `addCMComparison=True` (requires `useCMML=True`)

The analyzer runs in the ML job only. It consumes both `hgcalDigis` (raw) and
`hgcalCMCalibDigis` (ML-corrected) in the same event, reconstructs what the analytic
correction would have been (without applying it), and fills five DQM histograms under
`HGCal/CMComparison/`:

| Histogram | What it shows |
|---|---|
| `analyticCorrection` | Distribution of `cm_slope × (0.5 × CM − cm_ped)` — the analytic term that `HGCalRecHitCalibrationKernel_adcToEnergy` would subtract |
| `mlCorrection` | Distribution of `raw_adc − ml_corrected_adc` — the actual ADC change made by the DNN |
| `residual` | ML correction minus analytic correction per channel/event — non-zero means the DNN learned something beyond the linear term |
| `analytic_vs_ml` | 2D scatter of analytic (x) vs ML (y) corrections — diagonal = perfect agreement, off-diagonal = DNN adds non-linear component. The Pearson correlation of the two corrections is written into the plot title (to 3 dp) by `HGCalCMCorrectionCompareHarvester`. |
| `ml_vs_cmsum` | Profile of ML correction vs pedestal-subtracted CM sum — shows whether the DNN's primary response is the same linear trend as the analytic formula |

The analytic correction is **reconstructed from calibration parameters** inside the analyzer;
it is not applied to the digis in this mode. This lets you see what both corrections would
predict on the same event population.

#### `HGCalCMCorrectionCompareHarvester`

**Plugin:** `HGCalCommissioning/DQM/plugins/HGCalCMCorrectionCompareHarvester.cc`  
**cfi:** `HGCalCommissioning/DQM/python/hgcalCMCorrectionCompareHarvester_cfi.py`  
**Wired by:** `step_RECONANODQM.py` when `addCMComparison=True` (scheduled before the DQM saver)

A `DQMEDHarvester` that, at end-of-lumi / end-of-job, reads the merged `analytic_vs_ml`
histogram and writes the **Pearson correlation** between the analytic and ML corrections
into its title, to 3 dp:

```
Analytic vs ML CM correction (Pearson r = 0.987); Analytic correction [ADC counts]; ML correction [ADC counts]
```

The value is `TH2F::GetCorrelationFactor()` — the Pearson r of the binned distribution shown
on the heatmap. Running as a harvester means it acts on the fully-merged, run-scoped histogram,
so the number is correct regardless of the number of streams. An empty histogram shows
`r = N/A` rather than a spurious `0.000`.

To open the ML DQM file and browse the comparison plots:
```bash
dqmgui cm_comparison_20260619_191020/ml/DQM_V0001_HGCAL_R000112048.root
# or in ROOT:
root -l cm_comparison_20260619_191020/ml/DQM_V0001_HGCAL_R000112048.root \
         cm_comparison_20260619_191020/analytic/DQM_V0001_HGCAL_R000112048.root
```

---

## Debugging DNN Inputs

Config parameters let you dump the 21 DNN features to a **CSV file** for a chosen event and
module without modifying any C++. The dump is written host-side (`HGCalCMCalibrationProducer::
writeDebugCsv`); the device kernel does no printing.

| Parameter | Type | Default | Meaning |
|---|---|---|---|
| `debugCsv` | `string` | `""` | Output CSV path. Empty = disabled. When set, rows are appended for events passing `debugEvent`. |
| `debugEvent` | `uint32` | 0 | Event number to dump. `0` = disabled. `4294967295` (UINT32_MAX) = any event. |
| `debugModule` | `uint32` | UINT32_MAX | Dense module index filter. UINT32_MAX = all modules. |
| `debugMaxChannels` | `uint32` | UINT32_MAX | Stop after this many channels (by `chIdx`). UINT32_MAX = all channels. |
| `debugFedId` | `uint32` | UINT32_MAX | FED ID for hardware-address module selection. |
| `debugCaptureBlock` | `uint32` | UINT32_MAX | Capture block index for hardware-address selection. |
| `debugEcond` | `uint32` | UINT32_MAX | ECON-D index for hardware-address selection. |

Both `debugEvent != 0` **and** a non-empty `debugCsv` are required to produce output. If all
three hardware-address fields are set (not UINT32_MAX), they take precedence over `debugModule`
— the producer resolves the dense index host-side.

**Example: first 5 channels of dense module 2, first event of a file**

From `HGCalCommissioning/Calibrations/TB2025/maps/modulelocator_v6.txt`, dense module index 2
is `ML-F3WC-IH0198` for Run112049 (the `module` column in the CSV records the exact typecode,
so you never have to guess). Using `debugModule=2` avoids FED ID translation.

```bash
cmsRun $CMSSW_BASE/src/HGCalCommissioning/Configuration/test/step_RECONANODQM.py \
  era=TB2025/v6 run=112049 \
  files=file:/eos/cms/store/group/dpg_hgcal/tb_hgcal/2025/SepTestBeam2025/Run112049/8bd9f88c-a32f-11f0-8f95-04d9f5f94829/v1/RAW2DIGI_112049_10.root \
  maxEvents=1 knoise=-1 skipLC=True useCMML=True skipMeta=True \
  elossfile=HGCalCommissioning/Calibrations/TB2025/calib/hgcal_energyloss_setup2_v1.json \
  calibfile=HGCalCommissioning/Calibrations/TB2025/calib/level0_calib_params_v5.json \
  debugEvent=4294967295 debugModule=2 debugMaxChannels=5 \
  debugCsv=cmcalib_debug.csv
```

`debugEvent=4294967295` matches any event number, so with `maxEvents=1` you get the first
event without needing its run-level event number. The CSV has one header row plus one row per
(event, module, channel):

```
event,denseModIdx,module,ch,digi_adc,unconn0_raw,cm0,...,cm11,msubchidx,msuberxidx,cellfrac,unconn0,unconn1,unconn2,unconn3,ntoa,ntot
13525,2,ML-F3WC-IH0198,0,75,81,0.153809,...,-110.5,-2.5,1,1.06319,...,14,2
```

Filter later with e.g. `awk -F, '$3=="ML-F3WC-IH0198"'`. Note that `ntoa`/`ntot` are
**module-level** counts: every row for the same `denseModIdx` shows the same `ntoa`/`ntot`
(the count over that module's digis), not an event-wide total. `digi_adc` and `unconn0_raw`
are the raw (pre-pedestal) ADC values; the 21 feature columns are pedestal-subtracted.

Across multiple input files / streams the rows accumulate into the one CSV (a mutex serialises
appends); the file is truncated once at the start of the job.

---

## Known Limitations / Future Work

- **Unconnected channel positions hardcoded:** `{8, 17, 19, 28}` is correct for LD silicon
  modules. SiPM modules or future layouts may differ.

- **37 channels per eRx hardcoded:** Valid for LD modules; needs parameterization for mixed
  geometries.

- **Per-event module array upload:** `h_chDataOffsets`, `h_enabledErx`, `h_adcPed`, `h_cmPed`
  and `h_valid` are rebuilt and uploaded on every event. These are conditions-constant per run
  and could be moved into an ESProducer for better throughput.

- **`moduleIndexer.enabledErx_` is not the run configuration:** it is set unconditionally to
  the module type's full mask, `(1 << nErx) - 1`, in `HGCalMappingModuleIndexer.cc` and is
  never updated. The actual per-ECON-D mask lives in `HGCalConfiguration`
  (`feds[].econds[].enabledErx`, from `config_econds_*.json`) and is what the unpacker obeys.
  For any module configured with a partial mask (P5 and FNAL configs use `0x1f` and `0x7`),
  `nErx` is overcounted, the mean subtraction is mis-centred, and `FILL_CM` reads an eRx slot
  the unpacker never wrote. Not yet fixed; `HGCalConfiguration` sits on the same
  `HGCalModuleConfigurationRcd` already consumed for `HGCalCalibParamHost`, so the fix is one
  extra `esConsumes`.

- **Training-domain gap on `cm5`:** the model was trained with eRx 5 of `ML-F3WC-IH0182` dead
  in 37.5 % of events (`cm5` pinned at `-131.05`), and never saw a *live* eRx 5 at
  `cm0 < -14`. That configuration is ~58 % of run 112049. The feature-order fix is an
  implementation fix; this one can only be closed by retraining.

- **Feature importance baseline:** measured 2026-07-27 on run 112049 inputs (permutation,
  60k digis): `msubchidx` 22.6 %, `cm5` 21.8 % (the bug, now fixed), `unconn1` 15.2 %,
  `ntoa` 10.8 %, `ntot` 6.8 %, the real `cm*` columns 1.6–3.2 % each, `cellfrac` 0.3 %,
  `cm6`–`cm11` exactly 0 on LD. Worth re-running post-guard to get a clean ranking.
  Note: torch in CMSSW is built **without** NumPy support — `torch.from_numpy` / `.numpy()`
  raise; convert via `torch.frombuffer` and `.tolist()`.
