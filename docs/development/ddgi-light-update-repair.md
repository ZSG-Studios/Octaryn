# DDGI light-update latency and idle-budget repair — 2026-09-20

The user reported that DDGI light updates respond slowly and asked why light
updating is slow. This repair keeps the restored DDGI implementation and
corrects how the scheduler spends its budget and how the trace finds lights.

## Measured baseline (before this repair)

A CPU replay of the production scheduler with modeled GPU feedback
(`build/release-windows/tools/ddgi-light-update-diag`) and the user's manual
logs (`logs/client/manual-rtxgi-20260919-150359.log`,
`manual-selfhit-20260919-151059.log`) established the mechanisms:

- **Perpetual idle retrace.** Every non-solid probe retraced at the fixed
  0.25 s interior / 0.5 s sky cadence regardless of stability. In a fully
  static scene the fine volume scheduled ~24,000 probes/s and ~2.7 M rays/s,
  pinning the entire quality-3 DDGI GPU budget (0.5 ms per 1/60 s per volume).
  The manual logs show the same saturated pattern: `oldest_seconds` pinned
  near 0.5 s and retrace bursts of 1,277–1,369 probes (~100–130 k rays) with
  long zero-dispatch gaps in between. The restored pre-SRC scheduler slept
  probes 120 frames and never retraced clean sky probes; the live-response
  repairs replaced that with the fixed short cadence for every probe forever.
- **Synchronized burst waves.** Probes retraced together age out together, so
  eligibility arrived as whole-cohort bursts. The amortized batch gate then
  collected each wave into one giant dispatch: 35–47 % of frames dispatched
  nothing, and the rest dispatched up to 2,647 probes at once.
- **Scheduler CPU cost.** `ddgi_schedule` spent 0.31–0.98 ms mean per frame
  per volume (p95 up to 3.3 ms) on a 64,000-probe fine volume, including
  ~1 MB/frame of scratch allocation churn and three partial sorts.
- **O(volume) invalidation.** Every light add/remove scanned all 64,000 fine
  probes (~0.5 ms) even when the light's influence box sat 5,000 blocks away
  outside the volume; streaming selection churn calls this repeatedly.
- **Uniform light sampling over the fog-reach list.** The trace shader picks
  one uniformly random light from the entire resident list — every emitter
  within the 1,024-block fog reach — and scales by the list length. Per-light
  GI convergence therefore scales with the total resident light count, and
  all existing GPU fixtures ran with exactly one light, so the regression was
  invisible to them.
- **Response latency floor.** A torch wake waited on the amortized batch
  timer, then drained four spaced observations while the background cadence
  competed for the same saturated budget: 0.43–1.27 s in the replay model,
  1.5–2 s measured on DX12 in the response fixtures.

## Corrections

- **Stability-aware cadence with per-slot jitter** (`DDGISchedule.cpp`). A
  probe keeps the short converge interval (0.25 s interior / 0.5 s sky) for
  its first four observations after any disturbance, then holds a long idle
  sweep (2 s interior / 3 s sky). A slot-keyed ±25 % jitter de-synchronizes
  cohorts so eligibility drips in smoothly instead of arriving as bursts.
  Response cadence (0.1 s × four observations), wake lanes, history policy
  and sun-drift tracking are unchanged; abrupt environment changes still wake
  sky-visible probes immediately.
- **Urgent dispatch without the batch timer.** An urgent tail (fewer eligible
  probes than one intended batch) dispatches immediately and may borrow at
  most one batch of future credit beyond what is already owed, so continuous
  dirty work settles to the sustainable rate after one catch-up burst. A wake
  burst window doubles the intended batch so light changes drain in a few
  large dispatches. Deeper backlogs still amortize and the adaptive
  controller keeps honest per-work proportions.
- **Box-culled invalidation.** `ddgi_invalidate` now walks only the wrapped
  slots whose cells can fall inside box+radius, and returns immediately when
  the box misses the volume's coverage. A distant light crossing the
  selection boundary now costs nothing.
- **Reused scheduler scratch and a sleeping fast path.** The per-frame scan
  reads the 32-byte probe controls only for awake probes; sleeping probes are
  filtered from compact arrays. Scratch vectors live on the volume, and the
  chosen-map uses generation stamps, eliminating per-frame allocation churn.
  Wake-marker retirement scans only the marked list instead of the volume.
- **Near-field light finding for DDGI.** The published light list is now
  sorted by distance to the camera (explicit API sources first under stable
  ties), and each volume binds only the prefix of lights that can actually
  reach it (volume half-diagonal + light range + margin). Trace convergence
  for a given light now scales with the near field instead of every
  fog-reach source; the full list still serves direct local lighting.
- **Occupancy classification column cache** (`DDGIOccupancy.cpp`) removes one
  map search per probe from the classification walk.

## Results

CPU replay, same static scene and settings as the baseline (fine radius 20,
64,000 probes, quality 3):

| Measurement | Before | After |
| --- | ---: | ---: |
| Steady probes/s, zero scene changes | 24,436 | 4,421 |
| Steady rays/s | 2,736,793 | 495,186 |
| Steady modeled GPU ms/frame | pinned at budget | 0.066 |
| Zero-dispatch frames | 47.4 % (wave gaps) | 5.2 % |
| Schedule CPU mean / p95 per frame | 0.31 / 0.46 ms | 0.09 / 0.24 ms |
| Torch wake invalidation CPU | 0.52 ms | 0.06 ms |
| Far-light invalidation CPU | 0.50 ms | ~0 |
| Torch response drain (replay) | 0.43 s | 0.35 s |

The replay's torch scenario is an easy closed room; the production win is
larger because the budget is no longer saturated by background refresh when a
real wake arrives, and multi-light worlds no longer multiply per-light
convergence by the resident light count.

## Verification

- CPU: `validate_ddgi_light_publication.py` passes the publication fixture,
  the 36-case scheduler suite (new `static_idle_sweep` case pins that a
  converged static field settles into the idle sweep instead of saturating
  the trace budget) and the full response matrix at 30–2,000 Hz including
  its fixed-overhead, partial-tail, priority-only and environment-age
  negative controls. The former soft-only publication control is still
  rejected. `validate_lighting_changes.py` passes 1,416 checks, including
  that reordering the light list wakes nothing.
- Deliberate test recalibrations: cadence pins moved from the fixed 0.25/0.5 s
  intervals to the converge/idle model; the fairness bounds now cover one
  jittered idle interval; the dispatch-overhead throughput floor moved from
  900 to 700 probes per tick because idle-cadence batches run closer to the
  amortized target (the historical 2–5-probe collapse remains far below it);
  the partial-publication ladder extends past the amortized batch so full
  dispatches still form at high frame rates.
- GPU, Windows DX12:
  - Torch add/remove fixture (`torch-60fps/torch-fine16-coarse128-emm020vk`,
    fine 16 / coarse 128, 60 FPS cap, timing profile): all 24 tracked chamber
    histories advance in the cold, added and removed phases; receiver
    luminance moves 0.051 cold → 0.580 added → 0.054 removed with cold noise
    ≤ 0.0015; the sealed-interior leak case stays dark; DDGI trace steady
    median 0.153 ms; probe backlogs drain to zero and `oldest_update_seconds`
    settles at the idle sweep (median 3.7 s) instead of being pinned at 0.5 s;
    the adaptive fine budget holds a healthy median 734 probes per 60 Hz tick
    with zero-dispatch gaps gone (dispatch most frames). Cold, added and
    removed GPU images were inspected.
  - Dynamic 25-transition fixture (`dynamic-dx12-2/`, production scheduler,
    ray scene and shaders, 48-probe cohort): all transitions pass with
    sustained 80% response 0.28–1.46 s (slowest `sun_off`, previously the
    documented slowest case as well). A first attempt failed its final-tail
    sampling requirement during a concurrent build on the same machine; the
    uncontended rerun passes, consistent with the sampling-sensitivity
    incidents already recorded in the live-response and stability reports.
  - Moving natural-world capture sequence on the saved user view
    (`visual-motion/visual-dx12-0-native-jug5n10l`, DX12, native upscaler):
    4.24 ms median frame wall, 12.7 ms worst, 111 FPS at the 1st percentile,
    zero temporal resets. Steady-state DDGI dispatches a smooth trickle
    (0–14 probes per logged frame) instead of the former 1,277–1,369-probe
    bursts, `oldest_update_seconds` rests at the idle sweep (≈3 s) instead of
    being pinned at 0.5 s, and both adaptive budgets hold headroom. The
    eight-capture motion montage was inspected: fully lit terrain, sun and
    shadows, foliage and water render correctly with no dark-world, leak or
    reset artifacts.

Windows DX12 results do not qualify Linux Vulkan or macOS Metal runtime.
