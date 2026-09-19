# DDGI temporal stability — 2026-09-18

The user reports severe flicker after the live-response repairs. This supersedes
the earlier acceptance of low-light variation as a remaining limitation.

**Follow-up (2026-09-19):** the user then reported sun projected into dark caves
below tree canopies and a reverb-like echo when sources update. Results are in
[DDGI reverb and leak investigation](ddgi-reverb.md); the sealed-cave leak was
not reproduced and is now guarded by a permanent GPU fixture case.

## Reference and diagnosis

Online source inspected: NVIDIA RTXGI-DDGI
[`ProbeBlendingCS.hlsl`](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl).
It retains accumulated irradiance using observation-level hysteresis, reducing
history for lighting changes rather than treating unsampled frames as independent
observations. Its gamma encoding and threshold clamps are part of its different
storage/sampling contract; this repair does not copy those formulas into
Octaryn's physical linear-irradiance buffers.

Octaryn's `pow(hysteresis, elapsedSeconds * 60)` retention discarded approximately
84% of mature history after a 0.5-second gap at hysteresis 0.94. The elapsed gap
contains no new ray observations. Replacing history with each rotating sparse
ray sample produces visible changes even when the actual lighting is constant.
The newly functioning scheduler made these repeated changes visible.

The user's manual log, `logs/client/manual-ddgi-20260918-165726.log`, reports zero
invalidation and drained dirty queues throughout its final steady samples, with
coarse age near 0.5 seconds. Repeated whole-field invalidation is therefore not
established as the cause of that recorded steady flicker.

## Qualification

The evidence root is `logs/client/ddgi-stability/`. Baseline and repaired captures
use the same authoritative tunnel, fine radius 16, coarse radius 128 and explicit
60 FPS setting. `tools/validation/measure_ddgi_stability.py` compares adjacent
GPU image differences in the final eight captures of each lighting phase,
separately from genuine transition response and probe-retrace verification.

## Implemented history correction

`DDGIIrradiance.slang` retains mature history per executed observation. Sample-count
warmup, explicit changed/reset rejection and bounded reactive blending remain.
No one-sided brightness clamp or ambient boost was added. Shader fixtures verify
the stationary mean and variance across 0.016/0.5/1-second update intervals and
reject the former elapsed-time formula. Trace/update/seed compile for DXIL,
SPIR-V and Metal source. The reference source is pinned in
`tools/validation/ddgi_response_reference.md`.

### First measured stability result

Baseline: `before-retry/torch-fine16-coarse128-jnfqucf6`.
Repaired DX12: `after/torch-fine16-coarse128-num0wwzk`.
Repaired Vulkan: `after-vulkan/torch-fine16-coarse128-hdezar6x`.

Mean adjacent-frame absolute luminance difference, final eight removed-phase
captures, normalized 0–1:

| Receiver | DX12 before | DX12 repaired | Vulkan repaired |
| --- | ---: | ---: | ---: |
| Floor | 0.013119 | 0.001131 | 0.001053 |
| Left wall | 0.028389 | 0.001861 | 0.001856 |
| Right wall | 0.041305 | 0.001121 | 0.001101 |

The DX12 comparison reduces measured variation by 91–97%. Both repaired backends
refresh all 24 tracked chamber histories in each phase. The before/after removed
sequence montage was inspected: the former bright/dark wall changes are greatly
reduced. `comparison.json` retains cold, added and removed metrics. These are
image measurements, not claims of zero Monte Carlo variance in all scenes.

The 16-frame 2560x1440 stationary natural-world sequence
`natural/visual-dx12-0-native-i6lvue_f` was also inspected and has no temporal
resets. Visible animation remains active.

### Response follow-up

The first dynamic DX12 run passes its six-second bounds, but newly introduced
lights take 4.1–4.6 seconds: source addition only set a scheduling wake, not the
history-refresh timestamp. This requires explicit notification of actual new
lighting, independently of ordinary temporal averaging. The corresponding
native correction and final measurements are documented below; the slower
intermediate result is not the final response behavior.

The first Vulkan dynamic run stopped because fewer than three samples completed
in the final 0.6-second measurement window. This failed run is retained under
`dynamic-vulkan/` and does not establish a complete dynamic-suite pass.

The first baseline runner also encountered a truncated capture-log line and
accepted an incomplete filename. The parser now waits for a newline before
recording a capture; all 99 partial prefixes in the targeted regression are
rejected. The clean baseline rerun above is the comparison source.

## Final source-publication correction and qualification

Actual additions now request `refresh_history` through the production light
publication hook and scheduler. This advances the probe refresh timestamp with
light marker 1, without excluding its existing published sample as a removal
would. Unchanged/reordered sources and ordinary gentle scheduling wakes do not
repeatedly snap history. Consumed hard-removal markers retire correctly when an
addition overlaps; still-pending removals preserve their rejection.

The final native bundle is rebuilt (`logs/build/ddgi-stability-final-build.log`).
Final DX12 image evidence: `verified/torch-fine16-coarse128-yddsrtyw`.
`final-comparison.json` records removed-phase mean adjacent differences of
**0.001256 floor, 0.001505 left wall, 0.001042 right wall**, reductions of
**90.4%, 94.7%, 97.5%** relative to the reproduced baseline. Cold and added
phases also improve. All **24/24 tracked probes refresh in each phase**. Final
removed-phase GPU images were inspected (`final-removed.png`).

### Final dynamic GPU suites

| Backend | Evidence directory | Transitions | Sustained 80% response |
| --- | --- | ---: | ---: |
| Windows DX12 | `final-dynamic-dx12` | 23 passed | 0.024–0.776 s |
| Windows Vulkan | `verified-dynamic-vulkan` | 23 passed | 0.021–0.777 s |

Both use the production scene, scheduler and shaders. Cases include point, spot
and area lights; addition, movement, color, intensity, removal; sun/day/night;
opaque roof closure/opening; and resident lava emission/removal. New-light
DX12 response is now 0.157–0.281 seconds rather than the intermediate 4+ seconds.
The 48-probe cohort must genuinely retrace; response requires three successive
samples within 20% of its measured new level. This is cohort qualification,
not a guarantee of those latencies at every radius or on other GPUs.

Two earlier Vulkan dynamic attempts failed their sampling-cadence requirement.
The fixture unnecessarily read back the entire fine irradiance field each time
to measure 48 probes. It now reads only the enclosing cohort buffer span, using
the same cells, math, real timestamps, six-second phases, 0.6-second tail and
three-sample requirements. The subsequent full Vulkan suite passes without
weakening those requirements. Fixture optimization does not change client code.

Natural-world 16-frame 1440p sequences were inspected on both backends, with zero
temporal resets. Vulkan evidence:
`natural-vulkan/visual-vulkan-0-native-rh2rohtu`.

### Regression checks

- All **77 shader cases per target** pass for DXIL, SPIR-V and Metal source:
  `logs/build/ddgi-stability-shaders-{dxil,spirv,metal}.log`.
- Stationary production-shader mean/variance checks at three observation rates,
  explicit changes/resets, reactive countdown and negative controls pass.
- `validate_ddgi_light_publication.py` passes the real publication hook plus
  scheduler, including new sources, unchanged sources and hard-removal overlap.
  Its old-soft-only negative control fails as intended.
- Existing scheduler regression and response matrix pass.
- Native light-change detector passes 1,416 checks; completed ray publication
  remains single-consumption/idempotent under its production fixture.

Windows DX12/Vulkan were executed. Metal source compilation is not macOS runtime
qualification. The repair targets the reproduced repeated stochastic flicker;
finite-ray noise and genuine lighting transitions are still possible.
