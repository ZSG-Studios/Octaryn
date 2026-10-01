# Slang virtual geometry implementation ledger

The remaining large-scene ownership and hierarchy work is specified in
[Shared scene geometry scaling](scene-geometry-scaling.md), including the measured
Zorah reservation/root-page lower bounds and required runtime qualification.

This is an implementation in progress, not a production cutover or a performance
acceptance report. The fixed target is native 3840x2160, 240 rendered FPS
(mean and p99 <=4.17 ms), Bistro, 1,000 items, and 128 independent animated
100,000-triangle characters with 64 joints and eight morph targets. Material
quality, RT shadows and reflections remain requirements. No result below proves
that workload meets the target.

## Preserved control

The pre-change packaged client/server and assets are preserved under
`build/release-windows/client/virtual-geometry-baseline/bundle`.
The original executable SHA-256 is
`1DDB7D3279F2FB864DA5FFAC590815607795AFB3C79D4630344F8F4F4868AE15`.
`logs/build/virtual-geometry/baseline.patch` and `baseline-status.txt` record the
pre-existing dirty tree; those changes were preserved. The patch does not archive
untracked file contents. Existing source files have not been reset or cleaned.
The original native-4K capture is in
`logs/client/virtual-geometry/baseline/map-dx12-on-0-ur979sj1`.

During integration, `MapDrawBinding.cpp` was found reading beyond its 20-float
view uniform array. Lighting inputs now come from their actual renderer owners.
A fresh direct-render control with that correction is in
`logs/client/virtual-geometry/live/map-dx12-on-0-pka43aqa`.
These are capped image captures, not performance acceptance runs.

## Implemented components

- Standalone RHI indirect mesh commands, standard 64-bit buffer atomics, precise
  capability reporting and backend validation fixes: see
  [RHI implementation](virtual-geometry-rhi.md).
- Static cluster-DAG cooking through the pinned meshoptimizer cluster-LOD code.
  Clusters have at most 128 vertices and 128 triangles. Independently compressed
  64-KiB pages retain full 80-byte authored vertex attributes after lossless decoding.
  Version 2 also supports explicit POSITION-only source primitives with 12-byte
  positions and reconstructed flat triangle normals. Positions are not quantized.
  Source, metadata and page hashes reject stale/corrupt cooks.
- Bounded asynchronous page decoding, generation-safe GPU slots, pinned roots,
  upload-fence publication and consumer-fence retirement.
- GPU hierarchy selection, complete resident replacement groups, projected error,
  frustum culling, bounded requests, asynchronous feedback and indirect work.
- Slang compute rasterization for tiny opaque triangles, mesh rasterization for
  other triangles, root-UAV packed depth/visibility atomics, alpha test before
  visibility, perspective-correct reconstruction and explicit material gradients.
- Separate glTF animation import/sampling and GPU morph/skinning components with
  current/previous positions and conservative union bounds.

## Required map rendering integration (2026-09-30)

Production map opaque and masked geometry now enters the custom virtual geometry
owner for both monolithic maps and resident world tiles. The former direct,
indirect and dense-meshlet opaque dispatches have been removed from the world
frame. Unsupported mesh/64-bit-atomic hardware rejects map loading explicitly.
`OCTARYN_CLIENT_VIRTUAL_GEOMETRY` no longer selects an alternate renderer.

The loaded, optimized map and final material coverage generate a content key over
vertex bytes, indices and primitive coverage. This includes decoded external glTF
buffers. Cooks are stored under the engine preference directory in
`geometry-cache/v2/<key>.vgeom`, or `OCTARYN_CLIENT_GEOMETRY_CACHE_PATH` when set.
Imported source directories are not modified. Metadata and page checksums remain
mandatory; atomic cache replacement uses unique temporary names.

Each resident map owns selection and page residency. Tile page pools clamp to the
asset page count and the configured per-map maximum, and their conservative
reservations participate in the existing aggregate tile admission budget.
Root pages arrive asynchronously for tiles; startup readiness remains false until
the geometry is drawable. Two shared visibility targets are reused across maps,
with each tile resolving against the same G-buffer depth. This avoids a full-size
visibility framebuffer per tile. Monolithic maps retain GPU cluster binning;
coarse history occlusion remains disabled by default pending movement qualification.

Transparency uses the existing globally sorted forward material pass. Dynamic
items have their separate instanced gameplay renderer. Map shadows and reflections
consume paged VG ray geometry. Uncalled indexed map-shadow render functions,
their shader pipelines, and obsolete opaque pipeline fields have been removed;
none provides an alternate map primary opaque path.

This cutover does not establish large-scene instance virtualization. Zorah's
billions of instanced triangles require an object-space, instance-aware package;
expanding that scene into the existing world-space `MapModel` is not viable.
Sequential tile selection and resolve are also not a batched global instance
renderer. Validation evidence for this change must be recorded after the actual
build and GPU captures; the older evidence below does not qualify this cutover.

## Instance-preserving source packages (2026-09-30)

`octaryn_map_scene_cook --catalog source.gltf catalog.json` records source resource
hashes, every unique primitive, exact contiguous triangle windows, source materials
and every active scene node matrix. `--cook catalog.json [first_part part_count]`
prepares object-space parts with at most 65,536 triangles, checkpoints the catalog,
and reuses validated cooked parts on restart. Source resources remain untouched.
Exclusive catalog locks prevent concurrent checkpoint writers; a process crash
releases its lock. Decoded meshopt views use bounded scratch files and mappings,
including compressed source ranges beyond 4 GiB. Catalog readiness is separate
from source completeness and remains false until every part is prepared.

Zorah metadata contains 2,068 meshes, 3,163 primitives, 16,988 instances and 26,429
parts. The first measured 65,536-triangle part of mesh 2018 in the old full-attribute
representation produced 260 pages, all pinned roots, and 8,054,246 bytes on disk.
That result rules out a blind full-scene cook using derived normal seams. The v2
compact mode is selected only when source metadata proves POSITION is the sole
attribute; authored normals, UVs, tangents and colors keep the full representation.
Its renderer reconstructs the geometric triangle normal instead of smoothing it.
The same part in v2 produced 1,062 clusters, 25 pages with one pinned root, and
1,426,133 bytes on disk. Part cooking took 884 ms; the command including source
identity verification took 12.82 seconds with 462.2 MB sampled peak working set.
This is one bounded part measurement, not a full-scene size or timing result.

The scalar instance contract stores row-major affine world/inverse transforms,
inverse-transpose normal transforms and determinant orientation. Raster instances
share their object's page pool and selection buffers; reflected nodes reverse
winding, and normals/tangents follow the source transform rules. Bounded object
parts now select one complete LOD cut refined for the union of their instance
views, with a default requested error of one pixel. This remains sequential
per-part rendering, not the global batched scene ownership needed at Zorah scale.
Transparent parts use the existing forward composition with transformed instance
bounds and global distance sorting; their bounded object-space buffers are shared
between nodes. Source BLEND coverage remains in the VG asset for ray geometry.
Full Zorah cooking, streamed runtime coverage, collision and performance still
require separate completion and qualification; catalog generation is not those
results.

The complete prepared-scene loader admits at most 256 cooked parts, 4,096 nodes
and 250,000 instanced collision triangles. Source identity verification is capped
at 512 MiB of referenced files, and geometry/ray/forward/image reservations share
a 512-MiB admission envelope. These are explicit bounds for the first complete
instance fixture path. They do not admit the full Zorah source, even after all
parts are cooked.

The separate `SceneSession` path now routes larger catalogs through shared
`SceneGeometry` spatial planning and `SceneCollisionResidency` authority/prediction
geometry. The coordinated native build and spatial CPU probe pass; actual streamed
renderer/movement captures remain separate qualification. The planner
indexes original nodes and unique object parts, transforms bounds conservatively
under mirrored/sheared matrices, protects the actor region, and rejects incomplete
or over-budget neighborhoods without truncation. Each admitted part has one GPU
owner with an explicit set of original node indices. A shared page-decode scheduler
avoids creating worker threads for every part. Removed owners remain charged to
the budget until frame/ray fences and snapshot references retire. Node membership
changes invalidate the ray scene, while kept nodes share their part's reservation.

`--bounds catalog.json [first_part part_count]` prepares exact source triangle-window
bounds. `bounds_prepared=false` remains pending even when a primitive-wide metadata
box is available; this prevents unknown large parts from being treated as spatially
ready. Cooking and reuse also prepare exact bounds. Render startup and captures
require the complete requested neighborhood. The offscreen RT guard is complete
only when all original part/node pairs are resident with ready roots; a local
neighborhood cannot certify arbitrary distant reflections. The full 26,429-part
Zorah cook remains outstanding. Actual neighborhood admission measurements now
reject the current source-order partition: at the authored camera, 128 m render
coverage requests 26,426 parts, while the 24 m XZ collision query requests 89,074
part-instance pairs with a 1.49 TiB reservation. Preparing exact source-window
bounds took 264.24 s and peaked at 618,745,856 bytes; no extra geometry was cooked
after admission failed. Shrinking the XZ collision radius to 3 m still needs
1,936 pairs and 35,025,241,636 bytes.

The metadata-only radius sweep in
`logs/tools/zorah-import/scene-catalog-radius-sweep.log` isolates vertical
overfetch: the same 3 m query using XYZ selects 20 pairs and 223,226,340 bytes.
At 8 m, render selection still covers 4,135 parts and 457,315,806 instanced
triangles. These measurements require spatially coherent triangle partitions
and a vertically bounded protected collision query; reducing a radius alone
does not make the complete source a qualified playable world.

`octaryn_map_scene_cook --order catalog.json [first_primitive primitive_count]`
now builds a source-bound spatial permutation before cooking. Bounded external
Morton merges retain every original primitive triangle ID, record exact part
bounds, and leave materials, source files and scene-node transforms intact.
The catalog stores the permutation digest, and that digest participates in each
cooked geometry key. Client rendering and shared collision gather the same IDs;
loading verifies path confinement, content identity and complete unique coverage.
The generic 131,072-triangle fixture uses a 4,096-record sort window and 10 MiB
peak scratch, shrinking interleaved part widths from 1,501 to 1. Its source-aware
fixture cooks all 32 opaque/BLEND parts with two original instances and authored
normal/UV attributes (`height-order-self-test.log`). Full Zorah spatial ordering,
cooking and runtime coverage remain unqualified; the command is not automatically
invoked by the menu preparation action.

An isolated copy of the actual Zorah catalog now verifies ordering of its largest
primitive: all 32,054,609 source triangles across 490 parts, with unchanged source
SHA-256 hashes, original nodes and canonical catalog. The bounded command took
57.516 s, peaked at 641,744,896 bytes working set and 448,425,984 bytes private
memory, and sampled 3,113,003,792 bytes of scratch including mapped source views.
The permutation occupies 256,436,968 bytes; complete unique-ID verification took
22.39 s with 32,010,240 bytes peak working set. Mean part-bound diagonal decreased
from 1.80 to 0.66 m. At that stairs instance's center, the 3 m whole-scene query
decreased from 688 to 662 unique parts and 72.8 to 71.1 million instanced triangles;
8 and 24 m queries were unchanged. The authored spawn is about 80 m away, so its
3/8/24 m queries were unchanged. These measurements show bounded, source-complete
ordering, and also show that ordering alone does not solve full-detail density or
per-owner GPU admission. Evidence is in
`logs/tools/zorah-import/scene-zorah-spatial-large/measurement.json` and
`part-extents.json`; no full Zorah cook or runtime readiness is claimed.

Windows cache I/O expands long paths only at filesystem boundaries, preserving
portable serialized identities. A 307-character output directory passes the
131,072-triangle order, confinement, cancellation and scratch-budget probe in
`spatial-order-long-final.log`. Separate imported source/catalog Unicode long-path
and authority tests remain recorded by the import validation owner.

`logs/tools/zorah-import/scene-residency-probe.log` covers shared part reservations,
same-part node hysteresis, actor protection, complete-set budget failure, pending
bounds/cooking, mirrored/sheared transforms and XZ-only authority queries.
`scene-spatial-self-test.log` repeats the complete package fixture on that build.
The forward-buffer cleanup is implemented: monolithic maps and tiles upload
only BLEND vertices/indices, with separate draw offsets preserving original CPU
collision and cook geometry. Opaque/MASK geometry remains exclusively in VG pages.
The complete CPU suite passes in `map-forward-fixed-self-test.log`, including
bounded forward extraction, unchanged source geometry, material-sensitive cache
keys and concurrent cache repair. Packaged runtime memory measurements remain
separate from this CPU result.

Monolithic root-page startup uses its own bounded upload fence because the loading
menu can already own the main frame queue. After that fence completes, an explicit
stream handoff preserves resident handles and external consumers while retiring
the startup upload timeline. The residency probe passes 1,295,059 checks, including
rejection of pending uploads/retirements and adoption of a new lower-valued frame
timeline. The actual menu-to-world transition requires separate packaged runtime
evidence; a bare-map launch does not exercise the overlapping loading-menu owner.

The instance-selection helper passes 10,212 CPU checks in
`instance-selection-probe.log`: a near instance refines one shared DAG cut without
duplicating the far instance's parents, missing pages retain the complete parent
cut, roots and output bounds fail closed, and affine sphere bounds cover mirrored,
sheared and nonuniform transforms. The helper uses a conservative maximum scale
and FP32 cancellation padding. The active raster path now uploads scalar/std430
192-byte instance views to GPU selection; its two retained frame buffers are
charged at 384 bytes per original node in scene admission. Buffer growth waits
for that frame's fence and consumed readback. Object geometry no longer forces
zero-error full-detail selection. The ray owner uses the same affine union with
frustum culling disabled, retains complete snapshots when a finer cut exceeds
the unchanged budget, and resets admission when node membership changes.

Fresh DX12 and Vulkan GPU probes pass in
`logs/tools/zorah-vg/publication-union-gpu-dx12.log` and
`publication-union-gpu-vulkan.log`. Fourteen affine selection cases exercise fine
and coarse cuts, mirrored/nonuniform/sheared nodes, culling, incomplete children,
missing roots, bounded feedback, retained view uploads and achieved-error bounds.
Finite coordinates near `1e30` use scaled norms; positive error survives GPU
underflow. The real ray-payload probe uses two affine views whose raster frusta
exclude the geometry, while preserving every offscreen root and authored material
hit. The prior 10,212 CPU checks remain reference-math evidence, separate from
these GPU results.

`world_geometry_selection` reports the last completed raster selection's requested
and conservative achieved error. `world_geometry_ray_ready` reports achieved
error independently of the original request; `world_geometry_ray_selection`
also records the admitted selection threshold and instance count. These focused
GPU tests establish the active selection contract, not full-Zorah runtime,
movement performance, global page ownership or the target AAA workload.

The rebuilt SceneCook self-test passes source instance/material preservation,
mirrored/sheared transform math, exact part coverage, object-space compact payloads,
BLEND triangle coverage, catalog source-identity/root guards and cache reuse:
`logs/tools/zorah-import/scene-final-self-test.log`. The separate
`scene-final-integrity.json` records changed external-buffer rejection, concurrent
writer rejection and successful resume without rewriting valid geometry. These
are CPU/package checks; final renderer images and graphics validation remain
separate qualifications.

## Evidence and limits

`world-integration-all.log` records a successful `octaryn_all` build.
`gpu-core-dx12.log` and `gpu-core-vulkan.log` record RX 9070 XT offscreen
API-core-validation tests for indirect mesh/count/offset behavior, atomic winners,
selection, visibility/depth, masked discard, stale generations and deformation.
CPU topology/residency and cache-corruption probes pass. These focused fixtures
do not establish complete visual parity.

The Bistro cook contains 65,293 clusters, 6,103 groups and 9,407 pages in a
454,318,360-byte file. Its 4,352 pinned root pages require 272 MiB. Conservative
seam/material constraints prevent a small root set; that is an outstanding
optimization limitation, not hidden by the page budget. Both API stream probes
used only 4,356 slots, loaded fine pages, exercised two evictions and checked
GPU-decoded bytes against the cache (`stream-bistro-{dx12,vulkan}.log`).

Outstanding acceptance includes material bins, animated cooked
payloads and live instances, global instanced geometry, shared animated raster/RT
snapshots, complete grouped paged RT qualification, and complete workload
timing. NVIDIA/Intel and Linux execution remain unqualified; Metal is out of
scope. Build success and AMD Windows probes must not be presented as vendor parity.

## Measured performance (2026-09-28, RX 9070 XT, DX12, 2560x1440 wall view)

Two-phase HiZ occlusion and GPU-driven software/hardware cluster binning are
wired into the live prepare chain. Per-stage GPU timestamps
(`OCTARYN_CLIENT_VIRTUAL_GEOMETRY_TIMING=1`, four-deep query ring) isolated a
single-pathology regression: `Selection.slang` `compact_main` used a bounded
compare-exchange retry loop on one global counter, serializing ~19.5k surviving
clusters on L2 round trips for 18.4 ms. One `InterlockedAdd` per survivor
(capacity equals total cluster count, so the bounded CAS bought nothing) fixes
it to 0.008 ms. Stage timings after the fix: upload 0.005, reset 0.002,
depth-loop 0.198, compact 0.008, finish 0.003, copies 0.005 ms; the full
occlusion+binning chain is ~0.7 ms. Measured frame stages: opaque 1.26 ms
(meshlet control 1.37 ms), total GPU 12.6 ms (control 15.0 ms), dominated by
HDR ray-traced sun shadows (sun_trace 5.7 ms), which are unrelated to raster.
Evidence: `logs/client/vg-timing9`. Hybrid probe parity is unchanged
(4050/2/0 on DX12 and Vulkan).

## Live-camera stability fixes (2026-09-28)

Interactive flying exposed two flicker sources that static captures hid:

- Two-phase HiZ history falsely culls visible clusters under camera motion
  (coarse-mip bleed culls, the next frame heals, producing a blink loop).
  History culling is now opt-in through
  `OCTARYN_CLIENT_VIRTUAL_GEOMETRY_OCCLUSION=1` until the classification is
  fixed; it is worth ~0.4 ms opaque and ~2.3 ms HDR when enabled.
- Page eviction used a degenerate LRU: `GeometryStream::pump` referenced every
  resident page each frame and `PageResidency::reference` refreshed the LRU
  clock, so `evict_oldest` degenerated to page-id order and repeatedly evicted
  the same low-id pages whether visible or not (persistent pending pages,
  blink-reload). Selection now touches a per-page generation on the GPU for
  every emitted cluster, the feedback readback carries the used set to the CPU,
  and `touched` updates only from that use signal; `reference` only advances
  retirement fences. Moving-camera captures at the 384-MiB pool (the thrashing
  configuration) are now stable with pending pages down from a persistent 6 to
  3, at unchanged frame cost (`logs/client/vg-flicker5`).

## Research and provenance

- The [Nanite deep dive](https://www.wihlidal.com/projects/nanite-deepdive/) is a
  conceptual reference for hierarchy, streaming and hybrid rasterization.
  No Unreal Engine source is imported.
- The [Visibility Buffer paper](https://jcgt.org/published/0002/02/04/paper.pdf)
  motivates deferred attribute reconstruction. First-party visibility and
  reconstruction shaders are original implementations.
- meshoptimizer `9d9890c73011d75920af614485296d1e03e95448` (MIT), registered in
  `cmake/Dependencies/DependencyRegistry.cmake`: `demo/clusterlod.h` is compiled
  unchanged through `ClusterLod.cpp`; `GeometryCook.cpp` supplies engine-specific
  attributes, material partitions, conservative border locks and page packaging.
  Its original license remains in the dependency source and must accompany
  distributed binaries using it.
- Niagara (MIT) and vk_lod_clusters (Apache-2.0) are design references only.
  No source files from either repository have been copied or adapted into this
  implementation. No NVIDIA-specific acceleration structures are required.

All GPU implementation and tests use Slang and standalone Slang RHI. Production
rendering remains client-owned. Shared source decoding and scene collision
residency serve client prediction and server authority without rendering types.
