# Shared scene geometry scaling

This is the next implementation design, not a completed renderer or a claim that
complete large scenes are qualified. The current bounded importer, spatial ordering and small scene
runtime establish useful components. They do not establish full-source rendering,
reflection quality, movement performance or the stated AAA workload target.

## Current allocation problem

Admission currently sums conservative per-part reservations for raster pages,
ray geometry, selection views and forward BLEND data. Every selected leaf part
retains independent root pages and GPU resources. Consequently, large part counts
can exceed the scene budget before preparation starts, even when spatial selection
is bounded. Admission reservations are not observed GPU allocations.

The normal scene admission envelope is 512 MiB. Decoded pages are 65,536 bytes:
one pinned page per selected leaf already costs `leaf_count * 65,536` bytes before
metadata, materials, textures or ray geometry. Removing reservation constants
cannot remove this root-coverage floor. Shared physical pools and a hierarchy
above leaf parts are both required before changing admission policy.

Compressed cache size, decoded bytes, logical device allocations, conservative
peak reservations and driver committed memory must be reported separately. New
budget defaults require measurements from complete source coverage.

## Existing reusable owners

`SceneAssets` already shares texture resources, sampler resources and two page
decode workers. `WorldGeometryRaster` already shares two hybrid visibility targets.
Opaque and MASK geometry have no indexed map-rendering alternative; only BLEND
keeps bounded forward vertex/index buffers. The RHI already uses its D3D12MA
allocator on DX12. Adding another third-party allocator would not fix ownership.

Each part still creates a `MapRenderer`, `WorldGeometry`, `GeometryStream` and
`SelectionGpu`. That means a page buffer, cluster table, five selection pipelines,
four topology buffers, two sets of eleven selection/feedback buffers, and a
material buffer for each part. Each live ray owner also has its own fence, build
buffers, scratch and retained generations. Pipeline caching does not turn those
owners into one bounded scene allocation.

The world frame currently records each part's selection, visibility and resolve
before advancing to the next part. This makes shared selection scratch possible
as an initial step, provided feedback is copied into tagged retained readback
storage. It does not solve the eventual dispatch count for tens of thousands of
parts and instances.

## Required ownership changes

1. **Scene-wide page residency.** Add a focused `SceneGeometryPool` owner under
   `octaryn-client/Source/VirtualGeometry`. It owns one bounded physical page pool,
   one residency policy, upload scheduling and accounting. Assets register immutable
   metadata and receive handles; they do not allocate independent pools. Page keys
   include content identity and local page ID. Global slot generations prevent an
   evicted asset from being read through an old page table. Original instance
   matrices stay separate from object-space geometry.

2. **Shared selection resources.** Separate `SelectionGpu` pipelines and frame
   scratch from asset topology. Reuse pipelines and scratch across sequential
   part draws, with explicit barriers and feedback tickets containing asset and
   generation identities. A completed ticket must not update a retired or reused
   asset. Share the scene material table using explicit primitive/material offsets
   instead of duplicating one-buffer tables and texture-vector ownership per part.
   Later batched selection can replace sequential dispatch without changing page
   identities or material semantics.

3. **Global ray admission and scratch.** Add a scene ray build scheduler and a
   single peak-memory ledger covering published, pending, compacting and retired
   generations. Reuse scratch only after the relevant build fence completes.
   Keep object-space BLAS shared across original scene nodes. The world snapshot
   already builds the final TLAS from those BLAS; live scene builds can omit the
   redundant per-part TLAS while standalone qualification retains its local TLAS.
   Budget the actual complete candidate before allocation and retain the previous
   complete scene if a finer replacement cannot fit. Do not average away retained
   generations or independently consume the same global budget in many owners.

4. **A hierarchy above the 65K-triangle parts.** Introduce a versioned
   `SceneHierarchy` package with coarse object-space replacement groups that span
   leaf parts of the same material primitive. Build it bottom-up with bounded
   spatial batches, using existing leaf cooks and the pinned simplifier. Parent
   geometry must replace the complete child group atomically. Preserve material
   seams and lock exterior boundaries; reject a group that cannot be simplified
   within its error and memory limits. Pack coarse pages across groups where
   possible. Loading a parent must not require all descendant metadata or roots.
   Parent pages remain resident until every selected child replacement and its
   metadata are ready. This is what can remove the one-pinned-page-per-leaf floor.

5. **Retain the active conservative instance union.** `InstanceSelectionView` and
   `select_geometry_instances` now drive GPU raster selection and the ray
   multi-view overload. The union produces one shared complete cut refined for
   every instance, with conservative affine scale/cancellation bounds for mirrors,
   shear and nonuniform transforms. The default request is one pixel; actual
   achieved error and budget-admitted ray threshold are reported separately.
   Focused GPU tests cover view-buffer lifetimes, extreme finite coordinates
   and offscreen ray payloads. Preserve that contract when moving from per-part owners to
   global pages and cross-part parents. Raster frustum/history never determines
   the complete ray cut.

All five contracts are necessary parts of the full scene direction. Shared scratch
and page ownership are a reasonable first implementation slice, but do not justify
relaxing today's pre-cook admission or claiming large-scene readiness. Cross-part parents and
instance-union refinement must be measured on the real catalog before selecting
new global budget defaults.

## File responsibility map

| Owner / files | Intended responsibility |
| --- | --- |
| `VirtualGeometry/SceneAssets.*`, `SceneSession*` | Register immutable part metadata and original node bindings; publish lightweight asset handles, aggregate admission and retirement |
| New `VirtualGeometry/SceneGeometryPool.*` | Physical pages, global page identities, decode jobs, upload/consumer fences and one bounded ledger |
| `VirtualGeometry/GeometryStream*`, `PageResidency.*` | Extract reusable source decoding/residency behavior into the shared owner; remove per-part physical allocation |
| `VirtualGeometry/SelectionGpu.*`, `WorldGeometry*`, `WorldGeometryRaster.*` | Shared pipelines/scratch, tagged feedback, asset topology ranges and per-instance drawing |
| `MapWorld/MapRendererMaterials.cpp`, `MapRendererInternal.h` | Shared scene material bindings and lightweight geometry references; preserve BLEND composition |
| `VirtualGeometry/WorldGeometryRay.*`, `RayGeometry*` | Global build scheduling and reservation, scratch reuse, object BLAS lifetime and complete-cut publication |
| `Rendering/RenderBackend/WorldRaySnapshot.cpp`, `WorldFrame.cpp` | World TLAS, original node transforms, aggregate consumer lifetime and draw scheduling |
| New `VirtualGeometry/SceneHierarchy*`, `ScenePreparation*`, `SceneCatalog*` | Versioned parent/child coverage manifest, bounded cook/checkpoint/cancel, source and order digest identity |
| `VirtualGeometry/GeometryCook*`, `GeometryFormat.h`, `GeometryCache*` | Bounded parent geometry cooking, error/boundary validation and coarse page packing; version changes only when the encoded contract changes |
| `Shaders/VirtualGeometry/Selection.slang`, `Geometry.slang`, `RayExpand.slang` | Global addresses/material offsets, instance-union selection and the same generation/coverage checks |
| `VirtualGeometry/SceneBudget.h`, `GeometryBudget.h`, `ScenePreparation.cpp` | Replace independent owner reservations with proven aggregate admission only after shared allocations exist |
| `tools/Source/VirtualGeometryProbe`, `tools/Source/SceneResidencyProbe`, `tools/validation` | CPU contracts, GPU lifetime/material assertions, source-complete cook and actual runtime captures |

Keep implementation files focused and below 500 lines. Do not combine catalog
I/O, offline simplification, GPU allocation, scene selection and collision into
one replacement owner.

## Invariants and dependencies

- Source GLB/glTF/bin files stay unchanged. Catalog, order and hierarchy identities
  include source content and cooker versions; leaf ranges retain exact source IDs.
  A coarse parent represents a declared complete source domain. Its simplified
  triangles are not relabeled as the original full-detail triangles.
- Offline batches have explicit RAM, scratch and cancellation bounds. Checkpoints
  are atomic, cancel/crash leaves no published partial group, and resume validates
  source and order digests. Unknown or incomplete groups stay pending.
- A cut contains a parent or its complete child replacement, never holes or both.
  Original nodes, transforms, materials, MASK coverage, BLEND composition and
  generated-flat versus authored normals remain accounted for.
- Slot reuse waits for upload, raster, ray and retained snapshot consumers.
  Fence values from different fence objects are not comparable; use one common
  queue timeline or retain explicit fence identities. Preserve the qualified
  loading-menu startup handoff and never borrow its occupied frame queue slot.
- Every allocation, pending replacement and retired generation participates in
  the same budget. RHI logical bytes and backend committed usage are reported
  separately. Root coverage exceeding capacity fails before publication.
- Collision continues using exact bounded source triangles and the shared
  client/server swept-volume gates. Visual macro LOD is not a collision substitute.
  Movement into unprepared regions remains held until complete collision is ready.
- The global scene hierarchy needs metadata residency as well as vertex-page
  residency. Loading every leaf's cluster/group tables would simply move the
  large-scene allocation into metadata.

## Required qualification

First prove shared allocation and lifetime on a many-part fixture whose local page
IDs collide intentionally. Include multiple materials, OPAQUE/MASK/BLEND, authored
attributes and POSITION-only parts, mirrored/sheared instances, coarse/fine cuts,
missing child pages, budget exhaustion, cancellation, delayed consumers and slot
reuse. Check that resource counts scale with global capacities, not part count.

Then exercise cross-part parent replacement on a known complete source fixture:
no cracks, missing or duplicate source domains; identical material ownership;
correct normals and reflection orientation; complete offscreen ray coverage;
honest projected-error reporting. CPU tests must compare GPU union selection to
the existing reference helper, including near/far instances and singular rejection.
Run DX12 and Vulkan GPU assertions independently. Emitted MSL does not prove Metal
runtime behavior.

Finally cook and run complete large scenes through the world library with their
own authoritative saves. Record cold preparation, warm open, initial root coverage,
stationary memory, moving-camera residency, boundary crossing, collision holds,
retired bytes, RT publication/error and shutdown separately. Inspect actual images
and original-node/material coverage. Use the existing watchdog and memory guards;
do not raise budgets or shorten visible coverage merely to obtain a passing run.

Use the bundled Bistro fixture for matched image and timing controls, alongside
many-part stress fixtures and complete imported sources. Shared pools, a coarser
hierarchy, metadata streaming and active instance LOD require new GPU lifetime
contracts. These requirements remain open in the active NVRHI rewrite checklist;
this document does not claim the renderer rewrite is implemented or qualified.
