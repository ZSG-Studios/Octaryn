# Performance plan implementation and acceptance

Status checkpoint: 2026-09-27, through build18 and the named runtime results
below. This is a coverage ledger, not release approval. Historical results retain
their exact build identities and are not silently promoted to later binaries.
Paths below are relative to the repository unless otherwise linked.

**Implemented** means the production path exists. **Validated** always names its
scope. **Open** means the requested acceptance is not established. CPU fixtures,
shader compilation, a native build and a GPU smoke are different evidence.
The 16.7 ms and 8.3 ms milestones remain open for the agreed native workloads;
generated frames never count as engine FPS.

## 1. Reliable measurements

| Plan requirement | Implementation and evidence | Remaining acceptance |
|---|---|---|
| Repair CSV headers; separate reflection work | Schema2 records use common render-submission IDs. Reflection trace/reconstruction/composition have separate GPU timestamps. `FrameTimingLog.h`, renderer GPU profiling, `performance_summary.py`. | Keep coverage explicit for older captures; validate any newly added columns. |
| Actual ray counts, history rejection, residency, CPU execution and fence waits | Optional shader query/rejection counters, CPU wall/thread execution and wait columns, renderer byte estimates, DXGI process GPU usage/budget, RSS/peak RSS. `performance-hq.md`. | Diagnostic atomics invalidate timing qualification. Coarse Windows thread CPU samples cannot attribute sub-ms work. Resource byte estimates are not physical residency. |
| Repair movement harness and identities | Hidden bounded camera route uses authoritative-ready frame indices, fixed camera/light, motion/settle/cut/post-cut. `capture_map_world.py`, `summarize_performance_matrix.py`, `case_evidence.py`, `asset_evidence.py`. Build/shader/server/GLB/driver/settings/resolution and optional cook receipts recorded. | Historical missing cooked identities are reported, never reconstructed. The original 50.9 ms interactive workload is not proven reproduced. |
| Separate startup, rendering, movement, cuts, capture and compilation | Warmup and capture-adjacent exclusions; phase distributions; startup and shader-cache logs. Build17 DX12 three isolated engine-cache empty/reused pairs pass exact identities and cache inventories; renderer-ready median2593→819ms. `shader-cache-startup.md`. | OS/driver/filesystem cold/warm loading remains uncontrolled. Engine-cache reuse is startup evidence, not engine FPS. Other backends/modes require their own pairs. |
| Hardware shader/barrier/occupancy/memory investigation | Vulkan RGP hardware capture at `logs/client/rgp/hardware-mad7tbhz/frame.rgp`; portable tool pins centralized. CPU outlier analysis in `performance-cpu-outliers.md`. | Hardware analysis and reproducible attribution of remaining shader/barrier/occupancy/traffic costs are incomplete. Instrumented capture is not FPS evidence. |

The strongest completed timing set is the 72-run Windows DX12 build10 matrix:
native 1440p and 4K, native temporal AA and separately reported FSR Quality,
static/motion, reference/adaptive, three repetitions each. See
[performance-full-matrix.md](performance-full-matrix.md) and
`logs/client/performance-build10-full/matrix-summary.json` for identities,
per-run distributions, memory and exclusions. Native 1440p GPU mean is
16.198 ms static and 17.436 ms motion; motion p99 is 25.901 ms. Native 4K remains
well above 16.7 ms. The matrix does not qualify later build14 changes or Vulkan.
Reference sampling shares new resource storage/compaction; it is not an untouched
pre-change executable. Three-run observed spread is not statistical significance.

## 2. Ray tracing with preserved quality

| Plan requirement | Implementation and evidence | Remaining acceptance |
|---|---|---|
| Variance/confidence-guided work and tile classification | Adaptive shadow/reflection sampling; opt-in rough-reflection tile classification in `MapReflectionTiles.slang`. Matched DX12 evidence in `performance-matrix.md` and the full matrix. | Full image/motion acceptance remains open. Sparse mode improves mean in one route without improving tails; keep opt-in. |
| Slang temporal reprojection and edge-aware reconstruction | `MapReflectionHistory.slang`, `MapReflectionTemporal.slang`, `MapReflectionFilter.slang`; depth/normal/material/view checks and spatial filtering. CPU reprojection fixtures and DXIL/SPIR-V compilation. | Validate foliage/glass/glossy disocclusion and persistent noise across native/reconstructed modes and both backends. |
| One jittered shadow sample with bounded extra work; bounded rejection recovery | Adaptive shaders replace the fixed expensive response; original sampling is available through `--rt-reference`. Actual query counters are a separate diagnostic path. | Counter results and image quality must be tied to the exact binary/settings; compilation alone does not establish ray behavior. |
| Full-rate sharp reflections; reduced rough-reflection density | Full-detail reference retained; rough sparse tile mode is explicit. | Sparse reconstruction is not approved as a universal default. |
| Secondary visibility, alpha foliage, contact shadows and correct disocclusion | Existing ray material/alpha tests retained; multi-map geometry descriptors use raw byte-buffer loads on DX12. | The fixed-sampling sequence harness exists; complete material-focused image/sequence acceptance is still pending. Moving-item visual coverage is absent. |
| History bandwidth reduction | Depth-based position reconstruction and compact history storage; actual reference/adaptive paths share these resources. | Precision/quality and isolated bandwidth benefit need separate evidence; do not attribute all timing gains to storage packing. |
| Static AS compaction | Fence-safe query/compact/release through pinned RHI. Bistro BLAS measured 754,999,424 to 378,061,804 bytes. `MapRendererRay*`, `MapRayBudget.cpp`. | Isolated traversal-speed gain is not established by the memory reduction. |
| Native reference and pinned FSR | Native mode0, native AA mode1 and FSR2.2.1 Quality mode2 recorded separately. Original RT sampling remains selectable. | No SDK replacement or frame generation is part of this phase. |

The quality-only fixture aligns actual jitter, shadow/reflection sequence and
presentation delta to ready frames and rejects activation/phase mismatches.
Its timings never qualify FPS. See [map-quality-captures.md](map-quality-captures.md).
Reflection-only 2x2 history search remains experimental and off by default.

## 3. Assets, memory ownership and geometry

| Plan requirement | Implementation and evidence | Remaining acceptance |
|---|---|---|
| Offline BC7 cooker, central pin, content-addressed mips and metadata | `tools/Source/MapTextureCook`, `MapTextureCache*`, central `DependencyRegistry.cmake`; authenticated v3 DDS metadata/checksums. Native fixtures pass. Six build14 DX12 native1440 timing runs establish memory/image-stage reductions. Build17 four-view fixed-sampling comparison verifies phase identities;32 paired samples and native crops show no obvious codec-induced gross loss. | Lossless remains default. The timing difference is marginal;24-frame capture spacing does not qualify continuous ghosting/flicker. Vulkan/4K/reconstruction and broader material acceptance remain open. |
| Cache metadata before decode; strict packaging | Runtime reads validated cached dimensions/opacity before source decode. `StageMapTextures.py` rejects missing, stale, incomplete or corrupt required cook data. | Runtime diagnostic source fallback is not packaged-asset validation; keep those contracts separate. |
| Bounded parallel preparation/upload | Two CPU tile jobs, one upload builder; per-mip block-row texture and offset buffer uploads. `MapAssetPrepare*`, `MapAssetBuild*`, `TileSession*`. Default 8 MiB/1 ms upload budget with telemetry. | Time budget is soft between uninterruptible driver/material calls. Monolithic loading remains more serial. |
| Release source images/CPU geometry; consolidate duplicates | Encoded bytes released after last image-role use. CPU geometry released after collision ownership/upload fence; retained counts support AS. Shared tile textures and exact sampler descriptors, per-map strong ownership, weak-key pruning. Multipart hashing removes whole-image/DDS digest copies; CNG and portable identity/corruption fixtures pass and canonical build17 includes the changes. | Encoded limits86 MiB/image and 512 MiB/model are separate from 256 MiB prepared-tile output; parser, optimization and collision scratch prevent a hard process peak guarantee. Full raster/index/RT buffer consolidation remains partial; hash-copy removal has no measured startup/soak claim yet. |
| Map-switch collision ownership | Immutable owned collision arrays, cached-world teardown before geometry release, owner-thread publication. Host/native lifetime fixtures and 20-cycle DX12/Vulkan graphical switching pass. | Switching uses small contrasting fixture maps; large-map switching and long authoritative Bistro traversal remain separate. |
| GPU culling and indexed indirect draws | `MapRendererIndirect.cpp`, `MapCull.slang`; original direct reference retained. Exact frustum behavior and platform shader semantics exercised. | Native 1440p three-run route found indirect about 0.063 ms slower than adaptive direct; not a winning default. |
| Meshlets on supported hardware; measured tier selection | `MapMeshlets.cpp` and Slang mesh entry; material-preserving topology probe and DX12/Vulkan hardware smokes. Build17 DX12 native1440 route has three matched repetitions per reference/adaptive/meshlet variant; all meshlet logs prove32792 groups and one opaque submission. | Meshlet GPU mean17.74970ms is slower than adaptive direct17.69362ms on all pairs; retain opt-in. Broader hardware/content/backend selection and strict visual parity remain open. |
| Screen-size geometry detail preserving boundaries/silhouette | Pinned meshoptimizer cooks border-locked per-material levels; alpha geometry and full RT/collision geometry retained. `MapLodCache*`, geometry cooker, native LOD fixtures. | Route measured no win; LOD remains opt-in. Full visual silhouette/detail-transition qualification is open. |
| Real tile residency: priority, hysteresis, cancellation, uploads, eviction, collision readiness | Exact 137-tile Bistro cooker, shared textures, staged GPU/fence/BLAS readiness, camera/actor priority, collision worker preparation, generation snapshots and fence-safe retirement. Global transparent primitive sorting spans resident maps. Build15 DX12 Bistro 1,800-second renderer-camera route passes residency transitions and per-cycle process/GPU memory limits. Build16 DX12/Vulkan 2,400-frame RHI-validation routes both publish all137 tiles with fully resident captures and stable idle authority. | Actual authoritative Bistro traversal, settled tile-accounting plateau and Vulkan's long Bistro soak remain open; renderer-camera motion does not replace authority movement. |
| Pool UI surfaces and bound filter areas | `RmlLayers.cpp`, `RmlFilters.cpp`; cropped surfaces with blur margins and pooling. Exact RGBA parity at 1280x720 and build16 3840x2160 for menu/card on DX12/Vulkan. `logs/client/build16-core/ui4k-parity.json`. | Animated/nested panels, pool reuse under changing content and sustained allocation acceptance remain open. Static cases create three filter surfaces and retain them; they do not prove reuse under churn. |

Detailed implementation and CPU evidence:
[map-asset-performance.md](map-asset-performance.md). Native build11 fixtures all
passed: `logs/build/performance-build11-map-{tiles,codec,lods,stage}.log`.
The CPU tile round trip retains all 4,146,017 Bistro triangles in 137 tiles;
all prepared tiles use cooked textures, with maximum retained payload 102,024,172 B.
This does not establish graphical coverage or peak process memory.

Safe BC7 compresses 141 opaque-color variants; alpha-bearing color, normal and
data variants remain lossless. Payload decreases from 1,319,998,448 B to 841,716,572 B
(36.23%). CPU base RGB PSNR is 50.409 dB weighted; no tested alpha-threshold flips.
Opaque decoded alpha may be 254. These are CPU diagnostics, not image acceptance.
[map-texture-comparison.md](map-texture-comparison.md) gives existing manifests
and the ready runner; no recook or default-asset replacement is needed.

The six build14 timing cases in
`logs/client/asset-variants-build14/asset-pair-l4v1wjiz/timing.json` comprise three
matched fresh-process repetitions per codec, with identical geometry, executable,
shader/server identities, camera/light and native2560x1440 DX12 settings. All six
engine shader/pipeline caches hit; OS cache state is unknown. Median image-stage
time fell 1,971.31 to 1,317.01 ms, and texture payload fell 456.125 MiB. Sampled
DXGI process GPU usage fell 576 MiB; CPU RSS and peak RSS did not materially improve.
End-to-end authoritative readiness was worse, not a demonstrated loading win.
GPU mean decreased 0.108627 ms against 0.089211 ms observed three-run spread,
only a 0.019416 ms margin: this is a marginal result, not a major FPS or statistical
significance claim. Build17 subsequently supplied the scoped four-view image
review below; other resolutions/backends, continuous motion and later binaries
remain separate. See the codec document for complete distributions.

### Bistro streaming investigation

Build13 `logs/client/bistro-tiles-build13/tiles-dx12-4dlsi6k9/client.log` failed
material upload stage 2 after 128 tiles, with reported 128 bindless sampler slots.
Per-map sampler allocation was identified as the scale-specific exhaustion path.
`MapSamplerCache` now shares exact descriptors across the session, with 64 live
configurations, expired-key cleanup and strong per-map resources retained through
frame retirement. Sampler create/bindless and texture-descriptor failures now
have distinct diagnostics. CPU descriptor probe and syntax checks passed:
`logs/build/performance-map-sampler-{syntax,test}.log`. Build14's DX12 route
`logs/client/bistro-tiles-build14-route/tiles-dx12-jyuwc2r9` then exited 0 with all 137
IDs published, maximum 137 resident, 174 evictions, 8 cancellations and exactly one
live sampler configuration. No descriptor-capacity increase was used.

The route's 879 upload pumps respected 8 MiB, but 457 exceeded the 1 ms soft CPU target;
maximum was 8.901 ms. The earlier 135-tile smoke
`logs/client/bistro-tiles-build14/tiles-dx12-mvsaks33` reached 26.36 ms. The two runs
must not be conflated. Peak route tile accounting was 2,130,043,896 B; physical
process GPU usage and whole-renderer allocations are separate. Only two cycles
completed with no settled plateau samples; memory stability and authority
movement are explicitly unqualified. See the asset document for exact memory
samples and intentional outside-map camera coverage.

Read-only follow-up found no second deterministic upload failure. At failure the
remaining nine tiles contain 8,927,072 B raw geometry-buffer payload by GLB-header
accounting; shared textures were already resident. The last 2,123,190,368 B working-set
reservation was below 2 GiB and included transient AS build/compaction allowance.
This does not prove all subsequent allocations fit. Driver heap/staging overhead
is not the logical payload budget. Resource creation/material preparation can
exceed 1 ms, and unwanted in-flight tiles currently finish upload/BLAS before discard.
The explicit GPU budget can be configured; oversized wanted sets fail rather
than silently dropping required geometry or waiting indefinitely.

## 4. Host, physics and networking

| Plan requirement | Implementation and evidence | Remaining acceptance |
|---|---|---|
| Remove transport-critical disk I/O; immutable status reads | Published connection/status snapshots; production pose/input/ACK/action flow leaves disk mailboxes. `RemoteTransportClient.cs`, `SessionChannels.h`, host transport owners. | Low-rate lifecycle endpoint/shutdown files and some JSON wire payloads remain intentionally distinct from mailbox traffic. |
| Bounded typed in-process channels; separate authoritative server | Typed native/managed command/pose/action channels with loopback transport for singleplayer. Real listening-server probes pass with no production JSON mailboxes. | This is not client authority and not wholesale wire-protocol replacement. |
| Ordering, ACKs, conservation, reconnect and backpressure | Protocol8 retains same-client authority sessions and resets fresh identities. Build16 live tests pass reconnect, fresh reset, full256-event journal hold/resume and exactly-one drops/ACKs. Consumer polling drives receipt ACK; inventory mutation waits for outbound capacity. | No server-restart recovery, process-crash receipt persistence or authentication claim. Bound/conservation checks do not qualify arbitrary module behavior. |
| Reuse scheduler graphs/callback state | Ordered authority phases in one job, reused callback state and retained common Taskflow graph; nested workers use cooperative execution. | Broad concurrent scheduler/load behavior needs its own traces; graph reuse alone is not execution-cost evidence. |
| Awake items, spatial interactions, direct lookup | Awake set, 4 m grid, direct ECS lookup and bounded cell reuse. Real Box3D/module 100/1000/10000 workloads, three runs each, conserve and settle all items. | No 10,000 replicated/rendered-item scene: the current contract carries drop/grant/target events, not item poses. |
| Profile prediction/reconciliation under loss/latency first | Build15 protocol8 headless transport passes 0/50/100/200 ms configured RTT; impaired cases use ±10 ms one-way jitter and 2% loss. Earlier native DX12 200 ms probe records prediction/ACK/correction metrics separately. | Headless transport does not qualify native prediction at all RTTs or other backends. No new prediction algorithm is claimed. |
| Authority phase time and allocations | Ordered-phase wall/allocation counters, item motion probes and scheduler probes. `host-performance.md`. | Configured tick length is never substituted for measured execution cost. |

Evidence and limitations are in [host-performance.md](host-performance.md).
Real item runs: `logs/server/authority-items-f82goqb7`, `authority-items-6o8s4b6c`,
`authority-items-rvnys0ff`; native motion: `logs/server/item-motion-scale.log`.
Collision residency/lifetime fixture: `logs/server/collision-residency-u0q3ai99`.
Native DX12 prediction probe: `logs/client/native-network-l9z1k7uu`.
Build15 reconnect evidence is `logs/server/typed-session-b9qxm0t1`: same-client
resume advanced ACK150→488, with welcome ACK151 and one drop; a new client
received resumed=0/ACK0, advanced to ACK257 and produced one additional drop.
Build15 RTT0/50/100/200 evidence respectively:
`logs/server/typed-session-54ivm40h`, `typed-session-bofgek0t`,
`typed-session-yp1wpeip`, `typed-session-9c87f_n0`. Every case produced one drop
and action ACK with zero production JSON mailboxes. The full transport suite
passed in `logs/server/session-transport-jz2y0eeo`. These are correctness runs,
not CPU execution or native-prediction measurements.

The 1,800-second authority/transport traversal passed in
`logs/server/authority-traversal-4y9a4mvx/result.json`: 16,197.59 m, 688 cell
boundaries, 21 reversals, 78,071 poses and ACK107995/108000. It conserved the
single dropped item and observed its floor settling. Collision performed648
loads/644 evictions with zero failures. After300 seconds, RSS range was
1,523,712 B and slope496 B/s; private-memory range was1,536,000 B and slope479 B/s.
Both passed predefined64 MiB range and4 KiB/s slope limits. Its isolated
build12/protocol7 payload and synthetic33-cell world do not qualify Bistro,
client prediction or graphics.

The subsequent backpressure slice passes isolated journal/action and
item-conservation fixtures (`logs/server/replication-backpressure-journal.log`,
`replication-backpressure-items.log`) and native ABI layout checks
(`replication-capacity-abi.log`). Build16 live backpressure passes in
`logs/server/typed-session-c0hjjgm2`: journal256, one-second hold with47 continued
poses, then ACK257 and exactly one drop. Reconnect/fresh reset passes in
`typed-session-ql9m21x7`;200 ms RTT/±10 ms jitter/2% loss passes in
`typed-session-nhap0hj8`; the full transport suite passes in
`session-transport-c9lw0ezh`. Build16 bundle identities and ABI version2/32-byte
layout are verified in `logs/server/build16-bundle-identity.json` and
`build16-replication-abi.log`. These are headless correctness checks.

## 5. Acceptance and rollout gates

| Requested gate | Current qualification |
|---|---|
| Three matched repetitions; median/p95/p99/worst; CPU/GPU/memory | Complete for named build10 DX12 matrices, build14 BC7 timing pairs and build17 DX12 native1440 reference/adaptive/meshlet route. Each keeps exact build/workload scope; Vulkan and untested later configurations remain open. |
| Native 1440p/4K; reconstructed modes separate | Complete timing coverage in the 72-run matrix. Visual and sustained-budget acceptance remain open. |
| Static/traversal/rotation/cuts/foliage/glass/glossy/items/UI | Camera route and targeted capture tools exist; UI parity is qualified only for named cases. Full material motion/quality acceptance remains open; moving-item visuals lack a contract. |
| No ghosting/missing geometry/leaks/noise/detail transitions | Requires image and motion review on exact final binaries. Timing gains and small fixtures do not establish this gate. |
| Twenty A→B→A switches | DX12 `logs/client/map-switch-y8lmkasm` and build16 Vulkan `logs/client/map-switch-ku72k8b7` each pass 41 sessions/40 unloads. Vulkan tail RSS range13,721,600B, slope−23,815B/session; GPU estimate38,732,140B and OS usage123,772,928B stay constant. Small fixture maps, not a large-map switch claim. |
| Thirty-minute traversal and stable collision/memory | Synthetic33-cell authority/transport soak passed1800 seconds with explicit collision/memory bounds. Separately, build15 DX12 Bistro renderer-camera traversal passed1800.00854 seconds/55 cycles with per-cycle process/GPU memory limits. Build16 Vulkan Bistro short-route correctness passes; actual authoritative Bistro traversal and Vulkan's long Bistro soak remain open. |
|100/1000/10000 items;0–200ms jitter/loss | Focused real authority item workloads and build15 protocol8 headless RTT cases pass; build16 repeats reconnect,200 ms loss and live backpressure successfully. Replicated visual item scaling and all-RTT native prediction remain open. |
| Build `octaryn_all`; DX12/Vulkan separately | Canonical build18 passes (`logs/build/performance-build-18.log`); full Python suite145 tests passes (`logs/build/performance-validation-suite-corrected.log`). Named short DX12/Vulkan paths pass, but there is no blanket backend qualification. Linux/macOS runtime evidence is absent. |
| Keep only improvements above noise with preserved quality | Build17 reference→adaptive GPU31.87655→17.69362ms exceeds observed spread; camera-cut quality fails due amplified flecks/glints on both backends, and16.7ms is unmet. Adaptive remains a candidate requiring recovery repair. Indirect/LOD and meshlets have no measured route win and stay opt-in. BC7 saves texture memory with scoped four-view evidence; continuous-motion/platform gates remain and lossless stays default. |

## Rejected experiments and evidence limits

- Unrestricted BC7 color compression changed alpha coverage (maximum alpha
  error 250; 10,591 base alpha-threshold 128 flips). That candidate was rejected;
  the safe opaque-only candidate has separate manifests and remains unapproved
  pending GPU comparisons.
- Scalar SHA hashing regressed texture loading to 20.2 s. Windows CNG hashing
  replaced it; later image-stage medians are1.894–1.933s in existing fresh-process
  runs. This is not a controlled cold/warm comparison against the original 8.25 s.
- Shadow 2x2 history search caused a build13 opt-in DX12 GPU fence timeout at
  `logs/client/build13-isolation/search-on/map-dx12-on-0-gz2n8ir7`; paired reference
  passed. The experiment was removed and build10 shadow bodies restored for
  build14. This does not establish the compiler/driver root cause.
- Earlier similar-camera/sun captures and startup-shifted jitter phases do not
  establish strict pixel parity. The newer fixed-sampling mode is quality-only.
- Sparse average improvement does not qualify unchanged p95/p99 or native frame
  budgets. Indirect/LOD route slowdowns are retained as negative results.

## Final-run updates

- Build17's serialized23-process asset queue passed:
  `logs/tools/build17-asset-run-b6606d1619/result.json`. Its BC7 evidence is
  `logs/client/build17-asset-qualification/bc7-quality/asset-pair-ekt7tcp_`.
  All32 paired native1440 frames have matching sampling phases; four sequence
  sheets and native crops showed no obvious codec-induced coverage/detail loss.
  Full-image RGB channel MAE ranges0.05724–0.15260 codes; these metrics are not
  perceptual thresholds. Dark/gray glass and very dark chrome are also present
  in the lossless reference; codec comparisons do not certify that underlying
  material presentation. Capture stride24 and the moderately rough chrome rack
  do not prove continuous-motion or mirror-sharp reflection quality.
- Build17 meshlet timing:
  `logs/client/build17-asset-qualification/meshlet-timing/matrix-summary.json`.
  Three repetitions per variant use348 common frames with complete cook identity.
  GPU means are31.87655ms original-ray direct,17.69362ms adaptive direct and
  17.74970ms adaptive meshlet. Meshlets slow every paired run by0.04778–0.07143ms
  and add33,756,376 geometry bytes, despite lower encode CPU wall cost. They
  remain opt-in. Named scope is Windows DX12/RX9070XT/native1440 motion only.
- Build17 engine-cache startup:
  `logs/client/build17-asset-qualification/shader-cache/shader-cache-pairs-93aglzu2`.
  All three pairs prove empty0-file directories then identical86-file reuse;
  shader/pipeline misses46/38 become hits46/38 with no I/O/rejection/lock errors.
  Renderer-ready medians2593→819ms and map-pipeline930.53→110.85ms improve;
  paired session-clock readiness does not consistently improve. OS/driver/file
  cache state is unknown and these runs never qualify FPS. Full results and
  limits are in `shader-cache-startup.md`; independent review record is
  `logs/client/build17-asset-qualification/asset-review.json`.

- Build16 DX12 and Vulkan Bistro 2,400-frame RHI-validation routes both passed:
  `logs/client/build16-core/bistro-dx12/tiles-dx12-aa6ylsqc` and
  `logs/client/build16-core/bistro-vulkan/tiles-vulkan-ag1jzv24`.
  Each published all 137 IDs, with captures showing 137 wanted/resident and
  zero preparing/uploading; the coordinator inspected both images. DX12 logged
  161 evictions/6 cancellations and Vulkan 155/10. Maximum upload stayed at
  8 MiB; CPU pumps reached 72.718 ms/46.256 ms respectively. These validation runs
  overlapped headless host correctness work and are not FPS evidence.
  Both `idle-qualification.json` files show exactly zero authoritative X/Y/Z
  range over 2,280 logged frames after the first 120. This qualifies the short
  build16 zero-input Bistro idle case, not moving-player traversal or a new
  long memory soak. Vulkan long-soak acceptance remains open.

- Build15 DX12 Bistro renderer-camera soak passed and exited 0:
  `logs/client/bistro-tiles-build15-soak/tiles-dx12-b1o7jypn/result.json`.
  Actual route time was 1,800.00854 seconds over 55 cycles, publishing all 137 tiles,
  with 5,542 evictions and 161 cancellations. Uploads never exceeded 8,388,608 bytes;
  maximum upload CPU time was 19.737 ms, so the 1 ms budget remains soft. Peak tile
  accounting was 2,147,313,860 bytes under the configured 2 GiB limit.
  Across 55 origin-cycle samples, excluding the first 3 cycles for the stated
  memory gates, RSS range was 44,154,880 bytes and slope 364,051 bytes/cycle;
  renderer estimate stayed 2,160,948,616 bytes; process DXGI range was 67,108,864
  bytes and slope 615,914 bytes/cycle. All specified range/slope limits passed.
  `process_memory.qualified=true` is distinct from the still-false
  `evidence.memory_plateau_qualified`: no separate settled tile-accounting samples
  were obtained. This 640x360 route has intentional outside-map views and does not
  qualify 1440p/4K frame budgets, authoritative movement or all material visuals.
  Build15 idle drift was present; the build16 native support fix has the separate
  short canonical idle results above. The independent synthetic authority soak is
  reported separately above.

- Build14 `octaryn_all` passed (`logs/build/performance-build-14.log`). Ten
  serialized DX12/Vulkan RT cases passed with RHI validation: original sampling,
  adaptive sampling, opt-in reflection history search, and odd 641x361 inputs
  with high/low reflection tiers. Each includes GPU captures; evidence is under
  `logs/client/build14-ray-smokes`. These are correctness/counter captures, not
  FPS measurements or full visual acceptance.
- The build14 Bistro DX12 route published all137 tiles over two cycles, with
  174 evictions and8 cancellations, and exited0. The exact sampler cache retained
  one live descriptor configuration. Upload byte pumps stayed at or below8MiB;
  CPU pumps peaked8.901ms, so the1ms target remains soft. Evidence:
  `logs/client/bistro-tiles-build14-route/tiles-dx12-jyuwc2r9`. This short run
  does not qualify a memory plateau or authority movement. Its missing image
  exposed an insufficient capture requirement; it is not visual evidence.
- The earlier build14 startup smoke published135/137 tiles before its frame
  limit and exited0; it did not establish complete residency. Its upload CPU
  peak was26.360ms. Evidence:
  `logs/client/bistro-tiles-build14/tiles-dx12-mvsaks33`.

The host rows above include build15 protocol8 RTT tests, build16 live
backpressure/reconnect/200 ms loss regression checks and the isolated build12
1,800-second authority soak. Subsequent codec, map-switch and
graphical-soak results require their own exact evidence; the open gates remain open.

### Build16 UI capture and identity checks

Eight real GPU cases compare cropped/full-size menu and item-card filters at
3840x2160 on DX12 and Vulkan. All four paired captures have exactly identical
RGBA pixels. Cropped DX12 menu filter ownership falls from 100,270,080 to 57,016,320 B
(43.14%); item-card ownership falls from 100,270,080 to 1,769,472 B (98.24%). Vulkan menu
ownership is 57,016,368 B and card ownership is 1,580,592 B. These are sampled filter-surface
allocation bytes, not whole-process GPU memory or a frame-time claim. Root
inspected DX12 menu and Vulkan card output. Evidence:
`logs/client/build16-core/ui4k-parity.json`, `ui4k-cropped/run-c79z9fae`,
and `ui4k-reference/run-v5vfs9qs` under the same build16-core directory.

Build identity schema 3 also hashes client-side managed/native DLLs and runtime
configuration, preventing a changed transport/module binary from being hidden
behind an unchanged native executable. Historical schema 2 identities retain
their original narrower scope. The UI harness rejects output dimensions that
differ from the requested capture size; every case above is actually 3840x2160.


### Build17 startup diagnostics and material coverage

Canonical `octaryn_all` build17 passes. Relative to build16, the runtime change
adds opt-in CPU stage tracing around menu resize, fence wait, acquire, RmlUi
encoding, finish, submission and presentation. It is disabled normally and does
not change the render algorithm or watchdog thresholds.

The first build16 Vulkan native1440 foliage reference capture failed its existing
250 ms frame watchdog before authoritative readiness:
`logs/client/build16-core/quality-vulkan/quality-tjg6zfiu`.
Frames39/40 took748.699/467.703 ms; frame39 render time was745.287 ms with15.625 ms
sampled main-thread CPU execution. A previously logged pipeline compilation
completed before map load/frame0 and cannot be identified as this stall's cause.
The original failure remains unresolved. Build16 traced and untraced retries
passed, followed by build17's menu-traced smoke at
`logs/client/build17-menu-trace/map-vulkan-on-0-1ttbukfq` with the same watchdog.
Its longest logged menu intervals were24.017 ms present,22.402 ms acquire and
13.604 ms RmlUi encoding. These diagnostic runs do not qualify startup latency or
FPS; successful retries do not establish a fix for an intermittent failure.

The original sign/chrome cameras had insufficient material coverage. The corrected
views were inspected in actual GPU previews: the sign includes lettering, metal
frame and awning; the chrome view includes a thin wine-glass rack with specular
highlights. The rack's source roughness is89/255, so it does not qualify a broad
mirror-sharp reflection. Corrected reference/adaptive native1440 capture processes
pass on both backends under `logs/client/build17-sign-quality` and
`logs/client/build17-chrome-quality`. Image acceptance is recorded separately in
[render-quality-review.md](render-quality-review.md).


### Saved player initialization and the live session

The normal build17 launch exposed a real save-loading bug: the server correctly
loaded the existing pose, then unconditionally applied and immediately persisted
the map's fresh-player spawn. Evidence is preserved in
`logs/tools/live-20260927-115827-91d8ac40` and its referenced activity log. The session
was stopped; the original player file was restored byte-for-byte from the launch
backup after graceful server shutdown.

Build18 retains valid saved poses unless explicit diagnostic map-spawn override
is requested. Fresh worlds still use the manifest spawn. `octaryn_all` passes in
`logs/build/performance-build-18.log`. The actual-module regression at
`logs/server/player-spawn-sqw_gjuv` passes saved/fresh/diagnostic cases, including
saved coordinates outside the startup collision region and delayed collision
readiness. Its exact scope and limitations are in [host-performance.md](host-performance.md).

The normal DX12 client and dedicated loopback server were relaunched as
`live-20260927-120259-24198793`. Initial authoritative pose and angles match the
original save: (-19.361347,2.005883,-9.006876), pitch -0.06122823, yaw 1.6175797.
Client PID20040 and server PID29808 were live and connected at verification; server
owns127.0.0.1:17561, accepted the client hello, and finite authoritative source time
advanced during movement. `logs/tools/live-launch.json` records identities, saved
file backups and runtime evidence. This interactive build18 session currently
uses the adaptive candidate; it is not another controlled performance run. It is
left running as requested. Runtime process liveness is a point-in-time result.
