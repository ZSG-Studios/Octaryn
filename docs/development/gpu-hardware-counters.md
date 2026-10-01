# One-shot GPUPerfAPI reflection diagnostics

The default-off Windows DX12 diagnostic uses AMD's public discrete-counter API.
It is a hardware-attribution tool, not an FPS or frame-budget qualification run.
There is no Vulkan implementation or occupancy claim in this slice. The existing
RHI renderer records and submits every engine command.

`cmake/Dependencies/DependencyRegistry.cmake` pins GPUPerfAPI 4.4.0.5 and archive
SHA256. `tools/build/support/acquire_gpu_perf_api.py` verifies the archive and
every extracted file, refusing conflicting cached files. Headers and the DX12
DLL come from the same package under `build/dependencies/tools/gpu-perf-api`.
Nothing is machine-installed and no elevation is requested. CMake includes the
headers on Windows; the ordinary renderer never loads the DLL.

Set `OCTARYN_CLIENT_GPU_COUNTERS_PATH` to a new JSONL output to opt in.
`OCTARYN_CLIENT_GPU_COUNTERS_FRAME` selects the earliest renderer frame (default
240); `OCTARYN_CLIENT_GPU_COUNTERS_NAMES` optionally supplies 1-8 exact
comma-separated names. `OCTARYN_CLIENT_GPU_COUNTERS_DLL` must be an absolute path
if supplied, otherwise the centrally configured SDK location is used. Existing
output files are rejected. The capture helper verifies the DLL against its
pinned archive receipt and records SHA256.

The library initializes before RHI device/queue creation and opens the native
DX12 device with `ClockModeNone`. Reports enumerate adapter, SDK version, counter
names, descriptions, UUIDs, types, units and sample types. The current default
candidate list starts with GPUTime, then compute/cache/memory counters. Each
addition must keep `GpaGetPassCount()==1`; unsupported defaults are explicitly
excluded. Exact named sets fail if unavailable or multipass. Public streaming
and occupancy counters are not selected. Adjacent temporal frames are never
treated as equivalent counter passes.

The sample wraps the **whole reflection group**, including classification,
tracing, recovery and reconstruction executed by the selected path; it excludes
later HDR composition. It is not a trace-kernel-only sample. Eligibility requires
requested tile readiness and current ray coverage plus 64 stable scene-revision
frames. At most 120 frames after the target are allowed to reach that state.
Otherwise the report fails as unsupported/unready. This gate does not establish
earliest initial-playable readiness or qualify moving scenes.

RHI recording callbacks supply the active native primary command list. Retained
owners survive command-buffer recycling. Begin/end occur during recording;
engine submission and presentation ownership remain unchanged. A submitted
sample records its renderer frame and frame-fence value. Nonblocking polling
checks that fence, then SDK readiness; only ready samples read results. After
600 attempts or 10 seconds, timeout retains resources until the existing
renderer-wide shutdown drain. Unbalanced scopes or failed GPU drains terminate
without unsafe teardown. SDK context/session destruction precedes device
release. Report write/flush failures invalidate the capture.

Callbacks commit RHI barriers and invalidate binding state; profiling can alter
execution. Neither GPA GPUTime nor instrumented engine timestamps qualify
normal FPS. Build36 demonstrates actual RX9070XT DX12 counter collection without
elevation, not other adapters, drivers or backends.

The capture helper exposes `--gpu-counters`, `--gpu-counter-frame` and optional
`--gpu-counter-names`. Single-frame camera-motion diagnostics require no replay
equivalence. Actual camera, internal/output dimensions, ready frame, temporal
and reflection phases, fixed delta and renderer frame are recorded. The helper
joins that frame to GPU/lighting CSVs and, when requested, its later matching
fence retirement in `frame-retirement.csv`. Missing requested trace evidence
fails. Counter captures always have `timing_qualification=false`.

Official references: [usage](https://gpuopen.com/manuals/gpu_performance_api_manual/usage/),
[initialization](https://gpuopen.com/manuals/gpu_performance_api_manual/api_functions/gpa_initialize/),
[context flags](https://gpuopen.com/manuals/gpu_performance_api_manual/api_functions/gpa_opencontext/),
[command-list integration](https://gpuopen.com/manuals/gpu_performance_api_manual/api_functions/gpa_begincommandlist/),
and [matched release](https://github.com/GPUOpen-Tools/gpu_performance_api/releases/tag/v4.4-tag).

## CPU verification and version correction

Five renderer translation units passed syntax checks. The native fake-dispatch
fixture exercises the real owner's fence gating, typed results, enabled order,
partial-begin balancing, callback retention, timeout retention, one-pass gate,
metadata escaping, report-write failure and fail-closed unbalanced recording.
The original thirteen Python tests covered metadata, exact frame/fence joins,
disabled behavior and archive/extracted-file rejection. Evidence:
`logs/build/gpu-counter-{native-final,helper-final,syntax-final}.log` and
`logs/build/gpu-counter-probe/`. These fixtures create no graphics device.

Build35 stopped setup with `sdk_version_mismatch`, preserved at
`logs/client/hq200-build35/dx12/hardware/reflection/map-dx12-on-0-h7iw42is`.
The correct pinned DLL file/product version is 4.4.0.5, while `GpaGetVersion`
returns `[4,4,5,0]`. AMD's [release version](https://github.com/GPUOpen-Tools/gpu_performance_api/blob/v4.4-tag/CMakeLists.txt)
uses major.minor.update.build; [API arguments](https://gpuopen.com/manuals/gpu_performance_api_manual/api_functions/gpa_getversion/)
use major, minor, build, update. Build36 maps these orders explicitly in CMake,
probe and helper, preserving strict hashes/equality. The CPU-only real-DLL probe
`verify_gpu_counter_version.py` verifies API/resource mapping without initializing
GPA or creating a GPU device: `logs/build/gpu-counter-version-verified.json`.
Sixteen helper/SDK regressions and the native fake-owner fixture passed after
correction. Canonical builds35 and36 passed; hardware evidence follows.

## Build36 hardware evidence

Per-case SDK/DLL, adapter inventory, executable/shader identity, observations and
fence joins live under `logs/client/hq200-build36/dx12/hardware/`. Consolidated
receipt: `logs/client/hq200-build36/gpa-traversal-memory-audit.json`. Each successful
row is one pass, ClockModeNone, fixed sampling, 1280x720 internal and 2560x1440
output. These are diagnostics, not ordinary timing measurements.

| Case suffix | Renderer / ready frame | Values | Same-frame trace / filter ms |
| --- | --- | --- | --- |
| b4rv01l6 | 600 / 255 | GPUTime 1,635,840 ns | 1.57380 / .06160 |
| pawcpybz | 800 / 426 | CSWavefrontsLaunched 43,200 | 4.16948 / .06472 |
| a9if5rsa | 800 / 446 | CSBusy 99.717955%; waves 43,200 | 1.31564 / .06568 |
| a3fqtf47 | 800 / 546 | L2 hit 88.994536%; requests 49,254,351; misses 5,420,670 | 4.08256 / .06476 |
| km4ilm2b | 800 / 544 | MemUnitBusy 97.568796% | 3.99800 / .06492 |
| 1dexbwtx | 800 / 548 | MemUnitStalled .728203% | 3.83704 / .06516 |
| 6110mlps | 800 / 475 | RayBoxTests 140,668,974 | 4.27252 / .05684 |
| ezbq0kzt | 800 / 553 | RayTriTests 105,183,628 | 3.73808 / .05616 |

All renderer800 observations share post-cut camera
`[-.288867474,3,-20.1976242,.300000012,-.25,1.57079637]`, but sampling phases differ.
Renderer600 has a different moving-camera observation. **Do not sum box and
triangle values or divide them by waves from another row.** Timestamp columns
are joined within each case only and contain diagnostic effects. Their large
variation further prevents treating these samples as an FPS comparison.

Strict multipass failures remain preserved: `memory/map-dx12-on-0-m2az99le`
(compute/cache/memory), `memory-pressure/map-dx12-on-0-1ly4cqyp` (L2 and memory
busy/stalled), and `traversal/map-dx12-on-0-pi6767ws` (TotalRayTests and
RayTestsPerWave). Partial collection from rejected sets is not evidence.
GPUTime-first default selection excluded useful hardware additions because
hardware timestamps require their own pass.

The next source candidate defaults to **CSBusy,CSWavefrontsLaunched**, which
actually passed as one explicit set in build36. Use explicit named
singletons/supported sets for other questions and preserve the one-pass gate.
Use joined RHI timestamps for context, not GPUTime as the hardware-set anchor.

## Meaning and limits

- CSBusy means compute work is present; it is not occupancy or ALU utilization.
- L2 hit/request/miss share 64 raw events. Requests are not known VRAM bytes and
  do not alone establish bandwidth saturation.
- MemUnitBusy is maximum TA busy cycles over 64 blocks divided by CPF cycles.
  MemUnitStalled is maximum TCP-to-TA request-interface stall cycles divided by
  CPF cycles. Neither is an average across the chip. Low interface backpressure
  does not exclude other memory latency; high activity does not prove bandwidth
  is the bottleneck.
- RayTriTests sums eligible BVH4 triangle-node tests across 64 TD instances.
  RayBoxTests sums their FP16/FP32 box-node tests. These are hardware tests, not
  launched logical rays, shaded hits or unique scene triangles.
- TotalRayTests needs three events per TD, exceeding two discrete slots.
  RayBoxTests and RayTriTests singletons passed; their unmatched results cannot
  be combined into a hypothetical one-pass total.
- RayTestsPerWave uses `max64` of TD BVH4 instruction counts, without wave-count
  division. Its raw event warns that perf windowing is unsupported; the public
  name does not justify an arithmetic tests-per-wave interpretation.

The discrete scheduler's 120-event packing target permits a single derived
counter to exceed it. Busy/stalled need 65 events each, sharing only the clock:
129 combined events explain separate passes. Stage-specific SQ counters are
also isolated from affected texture/cache blocks. Do not alter SDK scheduling
or weaken the one-pass gate to combine incompatible sets.

The windowing warning alone does not imply whole-process counting: open PAL
GFX12 separately controls global and windowed counters at experiment boundaries.
That source does not prove proprietary Windows driver register programming.
Report supported GPA sample scope without stronger per-wave/window attribution.

Schema6 logical reflection counters cover the whole frame, including forward
glass outside the GPA group. Helper early-outs can skip a query; player/map
visibility can issue two. Exact tests per launched query therefore needs scoped
counters at actual TraceRayInline sites in the **same GPA sample frame**. Those
atomics perturb timings. Large counts alone cannot prove poor BVH quality:
matched geometry, rays, materials, camera, history and images are prerequisites.

Sources: [derived GFX12 formulas](https://github.com/GPUOpen-Tools/gpu_performance_api/blob/v4.4-tag/source/auto_generated/gpu_perf_api_counter_generator/public_counter_definitions_dx12_gfx12.cc),
[TD events](https://github.com/GPUOpen-Tools/gpu_performance_api/blob/v4.4-tag/source/auto_generated/gpu_perf_api_counter_generator/gpa_hw_counter_gfx12_td.cc),
[group capacity](https://github.com/GPUOpen-Tools/gpu_performance_api/blob/v4.4-tag/source/auto_generated/gpu_perf_api_counter_generator/gpa_hw_counter_dx12_gfx12.cc),
[scheduler](https://github.com/GPUOpen-Tools/gpu_performance_api/blob/v4.4-tag/source/gpu_perf_api_counter_generator/gpa_split_counters_consolidated.h),
and [PAL experiment boundaries](https://github.com/GPUOpen-Drivers/pal/blob/dev/src/core/hw/gfxip/gfx12/gfx12PerfExperiment.cpp).
