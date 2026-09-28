# Cluster geometry (mega-geometry class) for map worlds

Goal: continuous-LOD virtualized geometry for GLB map worlds — cluster-grain
culling, LOD selection and streaming for both raster and ray tracing — that
runs on all latest-generation GPUs (RX 9000, RTX 50, Arc B) through Slang and
standalone slang-rhi. DX12 and Vulkan, no vendor-locked feature in the
required path.

## What NVIDIA RTX Mega Geometry is

RTX Mega Geometry is not one feature but a cluster architecture: geometry is
preprocessed into small triangle clusters, ray tracing structures are built
and updated at cluster granularity, and continuous LOD is selected per frame
from a precomputed cluster DAG. The hardware path uses NVIDIA-only Vulkan
extensions (`VK_NV_cluster_acceleration_structure`,
`VK_NV_partitioned_acceleration_structure`), while NVIDIA's own samples
rasterize the same clusters through the cross-vendor `VK_EXT_mesh_shader` and
ship two open-source builder libraries (`nv_cluster_builder`,
`nv_lod_cluster_builder`). [NVIDIA Developer](https://developer.nvidia.com/blog/nvidia-rtx-mega-geometry-now-available-with-new-vulkan-samples/ "citation")
Reported production results: Alan Wake 2 saw 5-20% FPS uplift and 300 MB VRAM
reduction; Witcher 4 and CONTROL Resonant adopted it. [NVIDIA Developer](https://developer.nvidia.com/blog/nvidia-rtx-innovations-are-powering-the-next-era-of-game-development/ "citation")

## Cross-vendor foundation (required path)

meshoptimizer v1.2 (already pinned) ships `clusterlod.h`: `clodBuild` builds a
Nanite-style cluster DAG — leaf clusters, then recursively group, simplify
~50% with locked group boundaries (crack-free), re-split — emitting per-group
bounding spheres and geometric error. `clodBuildHierarchy` adds a spatial
hierarchy over groups for visibility acceleration. This is the same builder
NVIDIA integrated into its `vk_lod_clusters` sample, it is MIT-licensed, and
it runs entirely on the CPU at cook time. [zeux.io](https://zeux.io/2025/09/30/billions-of-triangles-in-minutes/ "citation"), [x-cmd meshoptimizer v1.0 release](https://www.x-cmd.com/blog/251217/ "citation"), [clusterlod.h](/build/dependencies/src/meshoptimizer/demo/clusterlod.h "citation")

Raster on all latest GPUs: mesh tasks (amplification + mesh shaders) through
slang-rhi — DX12 mesh shader pipeline and Vulkan `VK_EXT_mesh_shader` cover
RX 9000/7000, RTX 50/40/30 and Arc B; our amplification-culling pipeline
(slice 4) is already this shape.

RT on all RT GPUs: standard per-geometry BLAS plus a per-frame TLAS over the
LOD-selected cluster set (plain instancing, no vendor extension). The
NVIDIA-only cluster-BLAS path can later become an optional fast path behind a
feature probe; it is never required.

## Design

1. **Cook**: for each map primitive (material segment), `clodBuild` produces
   the DAG: clusters (meshlet-layout-compatible records), groups with error,
   hierarchy nodes. Cached beside the existing `.lods` cache, keyed by content
   digest. DAG nodes carry `parent` links so runtime selection is a local
   parent/child error comparison, not a traversal.
2. **Select (GPU)**: a compute pass over all resident groups computes
   projected screen error (`error * focal / distance`, same convention as
   `mapLodSettings`) and selects the DAG cut: a group renders itself when its
   projected error fits the pixel budget and its parent's does not. Selected
   clusters append to a visibility list plus an indirect task-dispatch count.
3. **Draw**: the existing amplification/mesh pipeline reads the selected
   cluster list instead of the dense meshlet grid; frustum/cone culling stays
   in the amplification stage. Cluster records keep the 80-byte meshlet layout
   (sphere + cone), so `mesh_main` is unchanged.
4. **RT**: phase 1 keeps full-detail per-map BLAS (current behavior, zero
   quality change). Phase 2 adds per-frame TLAS of selected cluster BLASes for
   RT LOD; full-detail always remains selectable.
5. **Streaming**: clusters page at group granularity; the selection pass logs
   requests for not-yet-resident groups (Nanite's append-request pattern), CPU
   streams LRU. This slice lands after selection/draw are proven.

## Slice plan (each verified like slices 1-4)

1. Cluster DAG cook: `MapClusterBuild` (clusterlod.h per primitive), cache
   file, unit validation (error monotonicity, boundary integrity), no runtime
   change.
2. GPU selection pass + selected-list draw on the meshlet path, fused with
   `MapCullSet`; parity + timing at wall view and spawn.
3. Cluster streaming (request buffer, LRU residency) for tiled worlds.
4. RT LOD TLAS (cross-vendor instancing) behind `OCTARYN_CLIENT_MAP_RT_LOD`.
5. Optional NV fast paths (cluster BLAS, partitioned TLAS) behind probes.

## Risks

- DAG blowup on hard-to-simplify content: meshoptimizer stops groups that
  simplify <6%, producing multiple roots; acceptable, selection handles it.
- Memory: DAG adds roughly 1x leaf data on top (~2x meshlet bytes); the
  streaming slice bounds residency.
- Per-primitive DAGs fragment selection work for tiny primitives: cook merges
  primitives below a triangle threshold into shared segments first.
