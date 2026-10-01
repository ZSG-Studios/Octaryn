# Forward glass: layer-aware reflection receivers

Source audit and implementation design, 2026-09-27. This document does not
describe an implemented queue, a graphics qualification result, or a budget
pass. Renderer sources were not changed for this audit. All future rendered
qualification uses 2560x1440 output; native and reconstructed inputs remain
separate measurements.

## Current cost and ordering

- [MapRendererDraw.cpp](../../octaryn-client/Source/MapWorld/MapRendererDraw.cpp),
  `append_forward` and `draw_forward` (lines32–66), selects every visible
  `Blend` primitive and globally sorts across resident maps by descending
  squared distance to the primitive AABB center. Equal distances use append
  order. Each primitive retains its original indexed triangle order.
- [MapRenderer.cpp](../../octaryn-client/Source/MapWorld/MapRenderer.cpp),
  lines50–67, enables source-alpha/inverse-source-alpha color blending,
  `Less` depth testing and no depth writes. Thus opaque depth can reject glass,
  but one glass layer does not remove subsequent layers through depth writes.
- [WorldMap.slang](../../octaryn-client/Shaders/Map/WorldMap.slang),
  `forward_main` (lines79–98), evaluates the authored material, rejects zero
  alpha, traces primary sun visibility and invokes `map_reflections` for every
  surviving fragment. Material sidedness and texture alpha remain significant.
- [MapReflections.slang](../../octaryn-client/Shaders/Hdr/MapReflections.slang),
  `map_reflection_sample_count`, `map_reflection_sample` and
  `map_reflected_radiance`, executes one direction below roughness0.08 and the
  selected tier's full direction count otherwise. A secondary hit can trace
  its own sun visibility. [ReflectionQuality.h](../../octaryn-client/Source/Rendering/Hdr/ReflectionQuality.h)
  specifies eight directions at tier3: the upper bound is one primary shadow,
  eight reflection and eight secondary visibility rays per surviving fragment.
  Direction/normal tests can reduce this actual count.

This path does not consume the opaque reflection queue or its temporal history.
Overlapping glass multiplies the work. Reusing the opaque G-buffer's single
reflection value for every glass layer would lose receiver identity and is not
a valid implementation.

The existing primitive-center sort is not exact per-pixel geometric ordering
for intersecting or self-overlapping meshes. First preserve its observable
ordering; changing transparency ordering is a separate correctness decision,
not an incidental optimization. Atomic fragment append order must never become
the alpha compositing order.

## Identity contracts

Within a frame, use `(pixel/sample, ordered draw ordinal, triangle, side)` for
exact fragment lookup. The current single-sample raster path needs pixel identity;
future multisampling must include sample identity. `SV_PrimitiveID` supplies the
draw-local triangle; `first_index / 3 + SV_PrimitiveID` identifies its triangle
in the map's full index buffer. Do not lose this base when splitting a draw.

Temporal identity must instead include **tile identity and residency/content
generation, primitive/triangle identity, side, and surface barycentric position**.
Match the continuous barycentric/local-surface location within validated
reprojection tolerance; do not require identical floating-point barycentrics
across camera motion. Normal, material, alpha, depth and motion/history validity
checks are still required. A triangle edge or changed tile generation may reject
history and obtain new samples; it must not reuse another layer's history.

Existing source contracts:

- [MapRenderer.h](../../octaryn-client/Source/MapWorld/MapRenderer.h),
  `MapForwardDraw`, currently carries only a map pointer, primitive index,
  distance and frame-local order.
- [MapDrawBinding.cpp](../../octaryn-client/Source/MapWorld/MapDrawBinding.cpp),
  line34, passes the primitive/material index in `mapUniforms[8].w`; it does not
  pass a stable tile generation or triangle base explicitly.
- [MapRendererMaterials.cpp](../../octaryn-client/Source/MapWorld/MapRendererMaterials.cpp),
  line25, stores `first_index / 3` in the material's padding field.
- [WorldRaySnapshot.cpp](../../octaryn-client/Source/Rendering/RenderBackend/WorldRaySnapshot.cpp),
  lines72–75, assigns map TLAS IDs from the current map-record vector index.
  This index can change during streaming and must not identify temporal history.
  The snapshot already retains map/BLAS owners through GPU use.

A new stable tile-generation key must be delivered at map publication. A raw
pointer or frame draw ordinal is not sufficient. The frame's lookup from that
key to current GPU descriptors/TLAS records is separate and fence-owned.
Hashes require full-key verification or collision handling; a hash collision
must never merge receiver lighting. Coplanar layers and duplicate triangles
remain distinct through primitive/triangle/side identity.

## Owner interfaces and pass sequence

Keep geometry/material draw binding in `MapWorld`. Expose collection of the
existing ordered draw list and drawing of explicit indexed subranges without
changing their relative order. Include their original triangle base and stable
asset-generation key. Avoid game item IDs or server ownership in these APIs.

Put pooled receiver storage, temporal lookup and trace/shade queue state in a
focused client `Rendering/Hdr/ForwardReflectionState` owner. Frame orchestration
in `WorldFrame` submits the passes through standalone RHI. Split Slang gather,
queue shading and composition responsibilities into focused files, preserving
the500-line source limit.

For each ordered batch:

1. Gather every fragment that survives the existing opaque-depth, sidedness
   and alpha rules. Store its evaluated surface, alpha, identity and position.
   Preserve raster material derivatives; moving texture evaluation into compute
   without their gradients would change filtering.
2. Classify and shade receivers through coherent intersection/material queues.
   Initially use the exact existing directions and counts to isolate submission
   changes. Preserve alpha-tested candidate handling and secondary visibility.
3. Re-rasterize the same original draw subranges for normal hardware alpha
   blending, looking up the exact fragment's lighting. This preserves the
   existing draw/triangle order without relying on atomic append order.
4. Retire/reuse batch storage only after its consumers complete. Frame snapshots
   retain map owners and descriptors until the submission fence completes.

Validated per-layer history can subsequently reduce fresh samples for rough
reflections. Sharp reflections remain full-rate, and disoccluded/new/changed
layers receive actual new ray samples. This is the largest likely structural
reduction; its magnitude and visual quality are unmeasured. Rejecting zero-alpha
fragments before sampling the remaining material textures is a smaller exact
candidate, provided alpha and derivative semantics remain unchanged.

## Bounded storage and additional batches

The requested final behavior is **additional ordered batches on overflow**.
No fixed layer cap may discard or merge excess fragments. Four uncompressed
80-byte records per native1440p pixel already require about1.10GiB, before
history or multiple frames; a full-frame fixed layer array is not a reasonable
default allocation. Actual record layouts and budgets require measurement.

An overflowing gather batch must not partially composite or publish temporal
history. Mark it incomplete, subdivide its original ordered draw range, and
process all resulting batches in the original relative order. A single draw
can itself overflow: subdivision must support triangle subranges, followed by
disjoint pixel/scissor regions when necessary. Each region must still see its
contributing triangles in the original order. Half-open pixel bounds prevent
double composition at region boundaries. Adjacent triangles retain the same
raster coverage rules.

A conservative progress bound is triangle count multiplied by scissor pixel
count and sample count: a single triangle contributes at most one fragment per
covered sample. This can prove that a leaf batch fits, but using this bound for
all normal batches may create excessive draws. Initial compact batches can use
measured occupancy, while overflow subdivision retains the conservative bound
as its termination proof. Atomic reservation order is not a stable continuation
cursor across rerasterizations; continuation must identify deterministic geometry
and screen ranges, not whichever threads happened to reserve slots first.

The pinned RHI continuation/control mechanism remains unresolved. It must
express dependent gather/count, subdivision, shading and composition batches
without losing fragments or hiding a CPU readback/fence stall. GPU-driven
indirect batch descriptors are a candidate; their bounded descriptor storage,
overflow handling, barriers and backend behavior need implementation evidence.
No fixed retry count can silently abandon remaining layers. Capacity exhaustion
must produce explicit diagnostics and a correctness-preserving continuation.

Executing an overflowing batch with the retained exact inline reference shader
would preserve layers and could serve as a diagnostic comparison. It is **not**
the user's requested final additional-queue-batch implementation and does not
establish a performance budget pass. Weighted blended OIT, nearest-layer-only
replacement, alpha thresholds, dropping secondary visibility, or merging layers
by hash are also outside this design.

## Required evidence

Record actual receiver/layer counts, triangle and batch counts, overflow depth,
peak retained bytes, gather/queue/compose times, actual primary/reflection/
secondary ray counts and history rejection. Explicitly measure any CPU/GPU
continuation synchronization. Compare full native1440p and separately identified
reconstructed1440p output with the unchanged ordered reference, including
overlapping panes, intersections, alpha textures, front/back faces, tile reloads,
camera cuts, moving reflected objects and deliberate capacity overflow.
Performance or quality acceptance remains open until those captures and matched
timings exist.
