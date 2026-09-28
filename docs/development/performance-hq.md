# Performance and HQ implementation evidence

The approved September 27 plan is in progress. Passing a build or a short capture
does not qualify every workload or platform. All timing evidence below is from
Windows, RX 9070 XT, and native 2560x1440 unless explicitly stated otherwise.
Generated frames are not included.

## Measurement contract

CPU, GPU and lighting CSV records now carry schema version 2 and matching
zero-based render-submission IDs. Corrected shifted CPU/GPU headers; reflection
trace, reflection reconstruction and composition have separate timestamps.
`performance_summary.py` rejects malformed records and excludes warmup plus
capture-adjacent frames. CPU wall time, thread execution time, fence waits,
estimated allocation size, OS process GPU usage/budget and process resident
memory remain separate. Windows thread CPU sampling is coarse and is not a
sub-millisecond execution profiler.

Each case records executable/shader/server identities, source map hash,
settings, resolution, adapter/driver, command and watchdog policy. Optional
actual ray-query counters invalidate timing qualification because atomics
perturb shader execution. Camera-only motion exercises translation, rotation,
settling and a cut without injecting input or changing authority.

## RT checkpoint

Build17's nine-run native1440 DX12 motion comparison measures GPU mean31.87655 ms
for reference sampling and17.69362 ms for adaptive sampling (44.49% reduction),
with adaptive p99 26.28830 ms. Meshlets are slower than adaptive direct drawing.
This is candidate performance, not approved HQ output: exact camera-cut image
pairs on both DX12 and Vulkan show amplified yellow awning flecks and brighter
chrome glints with adaptive recovery. The next sampled frame is closer to the
reference, but no-new-noise/highlight parity fails. See
[render-quality-review.md](render-quality-review.md) and the current disposition
in [performance-acceptance.md](performance-acceptance.md). Neither native frame
budget is met. The earlier results below keep their original build scope.

The strict fixed-camera rerun is now recorded in
[performance-matrix.md](performance-matrix.md): three runs per variant, identical
observed camera trajectories and captured sunlight, native 1440p DX12. Adaptive
GPU mean is 17.4474 ms versus 31.4988 ms reference (44.61% reduction).
Sparse rough reflections save another 0.8313 ms but do not improve p95/p99;
indirect drawing and geometry LOD do not win this workload and remain opt-in.
The earlier preliminary measurements below are retained as investigation history.

Three preliminary paired runs in `logs/client/performance-matched`, 480 authoritative frames,
120-frame warmup and three captures each. Compare original conservative ray
sampling (`OCTARYN_CLIENT_RT_REFERENCE=1`) with adaptive sampling, Ultra shadows
and reflections, no upscaling. The table is the median of three run-level
statistics, not a pooled distribution.

| GPU statistic | Reference ms | Adaptive ms |
| --- | ---: | ---: |
| Mean | 32.148 | 17.843 |
| Median | 33.724 | 18.591 |
| p95 | 40.258 | 22.752 |
| p99 | 42.902 | 25.377 |
| Median run worst | 73.650 | 29.719 |

Mean GPU time decreased 44.5%, beyond observed run-to-run variation. The harness
then exposed roughly 2 cm of camera drift and elapsed-time sun-angle differences
between variants. These are similar-workload measurements, not strict image
qualification; fixed presentation camera and lighting reruns are required. Neither
the 16.7 ms nor 8.3 ms milestone is met by this workload. Post-cut images have
nearby camera positions and no obvious missing geometry; comprehensive
motion, glass, glossy surfaces and disocclusion acceptance remains pending.
The reference switch retains original sampling but shares the new resource
compaction and history storage; this is not an untouched old executable.

Implemented: bounded variance-based ray counts, temporal/edge-aware reflection
reconstruction, depth-based shadow history, separately timed passes, and
fence-safe static BLAS compaction. The main map BLAS measured 754,999,424 bytes
before compaction and 378,061,804 bytes after. Per-pixel adaptation is present;
tile classification and sparse rough-reflection sampling are implemented as an
opt-in experiment (`OCTARYN_CLIENT_RT_SPARSE=1`).

## Loading and geometry checkpoint

Content-addressed cooked mip chains and integrity metadata are generated and
validated during packaging. Lossless cooked data remains the quality reference;
BC7 is opt-in pending image qualification. A scalar SHA implementation initially
regressed the texture stage to 20.2 seconds; Windows streaming CNG hashing
reduced a subsequent measured stage to 4.87 seconds. This is not yet a matched
cold/warm loading distribution. Encoded source images are released after upload.

GPU visibility/indirect draws and cooked projected-error geometry levels are
opt-in pending measured selection. Main-map full-detail geometry has 4,146,017
triangles; two offline levels contain 2,930,453 and 2,184,995. Runtime chooses per
primitive, preserves opaque material borders and retains full-detail alpha
geometry and RT geometry. This is not evidence of actual runtime LOD counts.

## Host checkpoint

Owned collision geometry prevents map-switch lifetime errors. The native
collision probe passed 20 A-to-B-to-A cycles (60 transitions); a prepared tile
add/remove fixture is separate from whole-renderer map-switch qualification.
Production pose/input/intent/ACK traffic now uses bounded typed
channels and loopback transport while keeping a separate authoritative server.
Spatial item lookup and awake-only physics replace repeated full scans.
Authority phase wall/allocation counters distinguish startup BVH work from
steady tick execution. Actual item/transport results and their limits are recorded
in [host-performance.md](host-performance.md).

Build 10 passes `octaryn_all`. The 17-tile GPU route passes separately on DX12 and
Vulkan with RHI validation: all tiles published, 48 evictions, 18 cancellations,
and six identical settled-memory samples per backend. These small synthetic
fixtures establish lifecycle correctness, not Bistro-scale streaming performance.
UI filter cropping preserves exact RGBA output against full-size reference
surfaces in the four menu/card/backend cases; captures were visually inspected.
The full DX12 client passed twenty A-to-B-to-A map cycles with stable renderer
and GPU allocation measurements; process memory stayed within the stated test
tolerance. Vulkan switching and the thirty-minute soak remain separate checks.

## Remaining acceptance

- Bistro-scale tile residency and dynamic authority-region runtime qualification.
- Meshlet performance selection; initial DX12/Vulkan GPU rendering checks pass.
- Full native 1440p/4K and separately reported reconstruction matrices.
- Motion sequences and focused foliage, glass, glossy and moving-item quality.
- Higher-resolution UI allocation and parity checks.
- Vulkan twenty-cycle map switching and thirty-minute traversal soak.
- Hardware shader/barrier/occupancy analysis; portable AMD tools are pinned in
  the central registry. A Vulkan hardware capture with instruction/counter data
  exists at `logs/client/rgp/hardware-mad7tbhz/frame.rgp`; hardware analysis is
  not completed, and this instrumented run cannot establish frame timing.
- Complete latency/loss, scale and reconnect acceptance.

Design sources: [AMD denoiser](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/denoiser/),
[SVGF](https://research.nvidia.com/labs/rtr/publication/schied2017spatiotemporal/),
[AMD mesh shader guidance](https://gpuopen.com/learn/mesh_shaders/mesh_shaders-optimization_and_best_practices/).
