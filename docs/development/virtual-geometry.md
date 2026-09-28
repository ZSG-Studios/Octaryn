# Slang virtual geometry implementation ledger

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
  64-KiB pages retain full 80-byte vertex attributes after lossless decoding.
  Version 1 uses the existing static world-space import. It does not quantize
  positions. Source, metadata and page hashes reject stale/corrupt cooks.
- Bounded asynchronous page decoding, generation-safe GPU slots, pinned roots,
  upload-fence publication and consumer-fence retirement.
- GPU hierarchy selection, complete resident replacement groups, projected error,
  frustum culling, bounded requests, asynchronous feedback and indirect work.
- Slang compute rasterization for tiny opaque triangles, mesh rasterization for
  other triangles, root-UAV packed depth/visibility atomics, alpha test before
  visibility, perspective-correct reconstruction and explicit material gradients.
- Separate glTF animation import/sampling and GPU morph/skinning components with
  current/previous positions and conservative union bounds.

The current monolithic-map integration is explicitly opt-in through
`OCTARYN_CLIENT_VIRTUAL_GEOMETRY=<absolute .vgeom path>`. It keeps the existing
full-detail ray scene and sorted transparency. It still loads the original map
for materials, collision and ray resources. It is not yet the replacement for
whole-map loading, rigid instances, world tiles, or animated character rendering.

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
payloads and live instances, tiled/global geometry, shared animated raster/RT
snapshots, portable grouped paged RT, production cutover, and complete workload
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

All implementation and tests use Slang and standalone Slang RHI. Production
ownership is under the client; server collision and authority remain unchanged.
