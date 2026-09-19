# DDGI live-lighting response repair — 2026-09-18

The subsequent user-reported flicker is addressed in
[DDGI temporal stability](ddgi-stability.md), which corrects this repair's
elapsed-time history weighting while retaining live scheduling.

The user reports stale or slow probe updates across lighting types. This work
keeps DDGI and investigates scheduling, history integration, source changes and
geometry publication. It does not reintroduce the withdrawn renderer.

## Reproduced baseline

`logs/client/ddgi-live-response/before/torch-fine16-coarse128-pdxabnbb` records
64 real GPU captures with authoritative cold/torch-added/torch-removed states,
fine radius 16, coarse radius 128 and both frame-slot parities. The montage was
inspected. The right-wall receiver changes from cold luminance 0.08258 to 0.37675
with the torch, then 0.01099 after removal. This is a history-dependent appearance,
not proof that the cold image was correct.

Inspection of actual probe records found all 60 sampled tunnel-interior probes
still using startup trace frames at or below 120 during captures 665–975. They
were active, not sleeping/open-sky probes. Cell `[0,162,4]` retained trace frame
111 until an edit caused trace frame 1073. Thus a visually stable cold capture
was not a converged-lighting baseline.

The initial runner requested a 60 FPS setting, but `--frames` qualification is
explicitly uncapped. Its observed capture-span rate was 164 FPS, including
readback overhead. It is an uncapped baseline, not a 60 FPS experiment. The
runner now opts into `--validate-frame-pacing` when a nonzero cap is requested;
that mode explicitly supports lighting qualification. Actual observation time
and probe ages remain necessary for latency measurements.

## Corrections

- Receiver visibility now looks from probe to biased receiver, matching stored
  distance directions. The opposite direction sampled the wrong hemisphere.
- Removed the mature, positive-only brightness clamp that biased stationary
  sparse-light integration downward. Irradiance normalization remains physical
  irradiance, with diffuse recursive feedback divided by pi.
- Native environment change tracking covers actual sun/ambient/sky/day-night
  inputs, with accumulated gradual changes and immediate discontinuities.
- Local-light changes compare full source descriptions, covering point, spot,
  area, color, intensity, movement, cone and rectangle axes. Old influence is
  rejected and new influence scheduled in both volumes.
- Geometry-change influence uses the configured trace distance rather than a
  four-voxel wake radius. Predicted geometry and rollback publish source changes
  immediately for occupancy caches.
- Scheduler fairness reserves executed update work for aged histories, with
  measured, bounded per-volume throughput and amortized dispatch batching.

Reference inspection includes NVIDIA RTXGI-DDGI `Irradiance.hlsl` and
`ProbeBlendingCS.hlsl` at commit `f33e496ca31b3f0eec1c4e2cbaa8bb620e337fa6`.
No vendor shader implementation was copied.

## Verification status

Production change-detector tests passed 213 checks. Production Slang CPU fixtures
passed constant-radiance tiers, sparse-light expectation, recursive enclosure
add/remove response and mirrored visibility; both former shader defects fail
negative controls. Shader targets DXIL, SPIR-V and Metal compile.

Native scheduler integration and the first combined GPU response run completed.
A production GPU fixture covers point/spot/area source changes, sun
strength, day/night, opaque closure/opening and resident lava emission using a
fixed 48-probe cohort. Its six-second response bound is a test bound, not a
claim of acceptable final interactive latency. Final screenshots and measured
latencies must accompany any completion claim.

## First combined runtime evidence

`dynamic-dx12/d3d12-phases.csv` under the evidence root records 23 measured changes
using the production ray scene, scheduler, trace/update shaders and a fixed
48-probe cohort. All cohort probes must genuinely retrace during each phase.
Three consecutive samples must reach 80% of measured final response. All cases
passed on DX12, with measured 80% times of 0.026–0.774 seconds. Point/spot/area
color, intensity, movement and removal, sun on/off, day/night, opaque roof
closure/opening and resident lava emission/removal were exercised.

The full-world torch case exposed an additional uncapped controller failure:
small dispatches attributed fixed GPU overhead to each probe, driving adaptive
budgets down to 2–5 probes per 60-Hz tick and retaining histories over 14 seconds.
Batching earned work into amortized dispatches removed the large fine-volume
collapse; actual runs are recorded under `final-uncapped` and `final-60fps`.
The remaining coarse-volume throughput degradation at uncapped rates is still
under investigation; these directory names do not establish final acceptance.

At the explicitly paced 60 FPS setting, retained non-solid probe timestamps
advance, both dirty backlogs drain, and logged ages are roughly 0.5 seconds
coarse / 0.5–0.85 seconds fine. GPU trace/update medians are 0.231/0.083 ms.
Tiny gradual environment progression no longer dirties both entire volumes or
extends global urgent bursts. Discontinuities still invalidate immediately.

Full-world captures also show ongoing low-light variation: paced final receiver
residual mean absolute luminance differences are 0.0132 floor, 0.0179 left wall,
and 0.0172 right wall, with cold-frame variation of comparable magnitude. The
earlier frozen cold image was not a valid convergence oracle. These image metrics
must not be presented as proof of complete GI visual convergence.

The torch runner now records a fixed 24-cell air cohort above the source. In the
original cold phase, **0/24 histories advanced over two observed seconds** (last
trace frames 37–111). In the paced repaired run, **24/24 advance in every phase**,
with cold trace frames 659–662 advancing to 966–975. An uncapped diagnostic run
also refreshes all 24 in every phase. `chamber-refresh-comparison.json` retains
these comparisons independently of final-image luminance.

Opt-in per-volume GPU timing identified the remaining coarse controller failure:
a 19-probe tail cost 0.07444 ms and cut the proportional budget from 494 to 53.6,
although the preceding 411-probe dispatch cost only 0.1182 ms. Fixed dispatch
overhead was being extrapolated as per-probe work. Evidence:
`timing-diagnostic/torch-fine16-coarse128-vdzrx23u/ddgi-timing.coarse.csv`.
The diagnostic is opt-in through `OCTARYN_DDGI_TIMING_PROFILE_PATH`; ordinary
play does not write per-dispatch CSVs.

## Completed controller correction and final verification

Partial eligible batches now carry their intended amortized size into the
fence-owned timing result. They do not poison the full-batch proportional cost
estimate. Their actual GPU time is still charged through work credit and time
debt, and expensive full batches still reduce the budget immediately.

Final uncapped production case:
`verified-uncapped/torch-fine16-coarse128-e8if5kwp`. In its last 50 measured
dispatches, coarse budget stays at or above 551 probes per 60-Hz tick and oldest
coarse age stays below 0.594 s; fine budget stays above 1,316 with oldest age below
0.533 s. Both dirty backlogs are zero. All 24 fixed chamber histories advance in
cold, added and removed phases. No tiny-tail collapse is present in this run.

Final paced production case:
`verified-60fps/torch-fine16-coarse128-heznfvvo`. All 24 chamber histories also
advance in every phase. Trace/update medians are 0.166/0.062 ms. Captured cold,
added and removed images were inspected. Low-light image variation remains:
final mean absolute receiver differences are 0.0157 floor, 0.0233 left wall,
0.0292 right wall. This repair establishes live probe updates, not noise-free
or exact final-image convergence in all scenes.

### Dynamic-source GPU results on the final controller

| Backend | Measured phase transitions | Sustained 80% response range |
| --- | ---: | ---: |
| Windows DX12 | 23, all passed | 0.020–0.767 s |
| Windows Vulkan | 23, all passed | 0.022–1.686 s |

Evidence: `verified-dynamic-dx12/` and `verified-dynamic-vulkan/`, each containing
the native log, per-sample cohort/trace-age CSV and per-phase response CSV.
Vulkan's slowest measured case was return to night. The fixture qualifies a
48-probe fine-volume cohort while both volumes operate, not an independent
coarse-field convergence guarantee. Public sun-color changes are not exposed;
local-source color changes, sun strength and sky/daylight were exercised.

### Performance and build checks

The final 2560x1440 Native AA moving natural-world run
`verified-natural/visual-dx12-0-native-_u2u9qap` has **4.332 ms median frame wall
time**, **6.829 ms p95**, and zero capture-sequence temporal resets. Its montage
was inspected. DDGI now intentionally batches at high frame rates, so its
near-zero per-frame median is not a meaningful work-cost claim; DDGI p95 is
0.343 ms and maximum 2.160 ms in that window.

- Native bundle: `logs/build/ddgi-response-complete-build.log`, passed.
- Dynamic GPU fixture builds and both backend executions passed.
- All 77 client shader cases pass for each of DXIL, SPIR-V and Metal source.
- Production lighting-change detector: 1,416 checks passed.
- Existing 35 scheduler cases and the response matrix/negative controls passed,
  including 30/60/144/600/2,000 Hz fixed-overhead and partial-tail workloads.
- Production shader response/visibility tests and negative controls passed.

These are Windows GPU results. Metal compilation does not establish macOS
runtime support, and Linux hardware execution was not performed in this task.
