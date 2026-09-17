# Split Radiance Cascades and hierarchical voxel tracing

## Active decision — 2026-09-17

The user replaces the DDGI direction with **Split Radiance Cascades (SRC)** for
diffuse GI and a shared **hierarchical voxel DDA** geometry-query subsystem.
This supersedes continued DDGI feature restoration. Existing crash corrections
and captured DDGI baseline evidence remain valid work, but are not SRC evidence.

The screenshot establishes native/default lighting controls: ambient **0.65**,
sun intensity **0.75**, fog distance **1024**, sky ambient floor **0.25**.
Existing explicit saved preferences retain their values.

## Verified references

- Freeman and Sannikov, *Split Radiance Cascades: Real-Time Global Illumination
  via Sparse Radiance Probes*: <https://arxiv.org/abs/2607.20384>.
- Chandra reference: <https://github.com/entropylost/chandra>, inspected at commit
  `0d46002d310a1a4630c8b24af83b7e098d08cf73`.
- `src/cache.rs`: integer quantized identity; spatial LOD; probe spacing;
  quarter-capacity cascades and fourfold angular growth.
- `src/algorithm/trace.rs`: visible-sample counts and offsets, hemisphere rays,
  hit-distance cascade assignment, secondary rays and shared directional deposits.
- `src/algorithm/rc.rs`: surface/secondary probe creation, lifetime propagation,
  ray splitting, weighted interval accumulation and eight-parent far-to-near merge.

This is an independent C++/Slang implementation of the verified algorithm and
the user's voxel adaptations. The reference's Rust renderer, hardware geometry
backend, finite packed coordinate range and hard-coded 32-lane merge are not
the Octaryn architecture. Wave32/Wave64 behavior must remain correct.

### Supplemental user-provided 3D RC example

The user also supplied a GLSL final-shading excerpt headed “Radiance Cascades
3D,” combining surface irradiance/radiosity with five cubemap cascades. Its
comments describe visibility-weighted spatial merging, parent-ray interpolation,
and flicker from static probe placement and temporal cascade merging. They
explicitly identify naive smallest-probe bilinear interpolation as a source of
near-geometry light leaks.

Use those failure cases in the thin-wall, moving-emitter and camera-motion
qualification, and account for visibility in final probe reconstruction as well
as parent merging. The initial excerpt does not contain cascade construction/merge
passes, `TraceRay`, geometry layout or its supporting buffers. The subsequently
provided source link is <https://www.shadertoy.com/view/X3XfRM>. Direct retrieval
returned HTTP 403. A public secondary copy was inspected in
<https://github.com/uziiDevelopment/providentia-cascades> at commit
`542e3d6e18d083d0bc6f4e6e8d5f9d4a3ab4c7c3`, under
`reference/shadertoy/{common,cube_a,image}.glsl`. It identifies those files as
copies of the original; direct equivalence to the currently hosted Shadertoy
could not be independently checked.

The copied Cube A pass confirms previous-frame cubemap feedback for parent
merging and a traced-distance/flat-surface visibility approximation in
`WeightedSample`. Common supplies an analytic hard-coded room tracer. Use these
as supplementary algorithm/failure-case references, with current-frame C4→C0
merges and voxel-aware visibility in Octaryn. Its fixed cubemap dimensions,
GLSL entry point, demo camera and ACES/gamma presentation do not replace the
approved sparse SRC algorithm, Slang path or existing HDR/FSR presentation.

## Owners and data path

| Owner | Responsibility |
| --- | --- |
| `octaryn-client/Source/Rendering/VoxelTracing/` | Resident integer chunk identities, material voxels, occupancy, epochs, bounded updates and RHI uploads |
| `octaryn-client/Shaders/VoxelTracing/` | Shared segment query, world/chunk/macro/brick/block traversal and traversal diagnostics |
| `octaryn-client/Source/Rendering/SplitRadianceCascades/` | SRC settings, bounded allocations, cache lifetimes, scheduling and pass resource ownership |
| `octaryn-client/Shaders/SplitRadianceCascades/` | Sparse probes, rays, split deposits, merge, contact, evaluation and debug views |
| Existing world-presentation owners | Authoritative terrain reconstruction, accepted edits and reversible predicted overlays |
| Existing rendering/HDR owners | G-buffer inputs, graph dependencies and final diffuse composition |
| `tools/validation/` | DDA boundary/reference tests, actual GPU scene capture and quantitative comparisons |

The trace hierarchy is sparse world chunk lookup → macro occupancy → 4³ leaf
bricks → individual material voxels. One brick occupancy mask contains 64 bits;
the portable GPU representation may use two 32-bit words. Edits update material,
leaf and parent occupancy plus a geometry epoch without waiting for remeshing
or BLAS construction. Unknown/nonresident geometry is distinguished from known
empty space; trace exhaustion must not silently become unoccluded sky.

Global identity uses integer coordinates. Camera-relative floats describe ray
positions within a rebased integer frame; rebasing does not change cache keys.
Segment queries define signed coordinates, exact boundaries, zero components,
inside-solid starts, chunk crossings, long rays and finite traversal budgets.
Material geometry must preserve cutouts, fluids and existing shaped surfaces;
an occupied voxel alone does not establish a full opaque cube intersection.

The same trace subsystem is intended for diffuse GI, direct visibility and
reflections. SRC owns diffuse illumination, not mirror reconstruction. Optional
triangle entity queries can later combine by nearest distance. Voxel diffuse GI
must execute without hardware-ray-tracing capability.

## SRC algorithm and prototype settings

1. Seed sparse C0 probes from visible G-buffer surfaces, then create parents.
2. Count/distribute surface-generated hemisphere rays across probe directions.
3. Trace using the shared voxel hierarchy; evaluate emission, current direct
   diffuse light and previous SRC feedback at hits, environment on known sky misses.
4. Split each ray by first hit distance. Before the blocking interval deposit
   `(L=0,T=1)`; at the hit interval deposit outgoing radiance with `T=0` for opaque
   surfaces. Misses resolve through the final environment interval.
5. Merge far to near with spatial and directional interpolation using
   `L = near.L + near.T * far.L`, `T = near.T * far.T`.
6. Evaluate a short-range C-1 DDA contact layer and final diffuse irradiance.
7. Retain secondary probes from hit surfaces with bounded lifetime/quality;
   locally refresh geometry-epoch changes. No unbounded permanent cache.

| Runtime tuning | Initial target |
| --- | --- |
| Base voxel / C0 spacing | 1 block / 1 block |
| C-1 | 0–1 block |
| C0–C4 intervals | 1–4, 4–16, 16–64, 64–256, 256–environment |
| Interval growth / spacing growth | ×4 / ×2 |
| Cascade count | 5 |
| Base angular resolution | 4, yielding 32 directions for the reference's 2:1 angular grid |
| C0 probe capacity | 65,536 |
| Capacity / direction scaling | ÷4 / ×4 per cascade |
| Visible / secondary lifetime | 2 / 12 frames |
| History decay | 0.97 prototype value |
| Secondary probes, multiple bounce, contact | Enabled targets |
| Storage | FP32 until correctness is established |

Five cascades at that capacity/angular count have 10,485,760 directional slots.
One FP32 `float4` interval alone is **160 MiB**; weights, previous/merged fields,
metadata, hash tables and work queues are additional. The user's approximately
84 MB estimate applies to an eventual 8-byte packed interval, not this FP32
prototype. Allocation must be checked against an explicit byte budget.

Probe spatial LOD is distinct from radiance interval cascade. Blend neighboring
probe LODs around the viewer. The requested 4/16/64-block far-field occupancy
hierarchy extends tracing beyond exact residency and needs generator/edit-aware
occlusion. It is a lighting representation, not permission to simplify visible
raster meshes. The infinite final interval means an environment boundary with
defined far-world coverage; it is not an infinite GPU loop.

## Required diagnostics and acceptance

Expose hierarchy level and step count, hit voxel, empty-space skips, probe density
and LOD, cascade/angular bin, hit cascade, transmittance, age/quality, collisions,
occupancy/overflow, secondary status, geometry epochs and isolated C-1/C0/C1 views.
Track rays/frame, chunk/macro/leaf/block steps, memory, misses and GPU pass timing.

Use a slow high-sample reference driven by the same voxel query. Compare actual
GPU captures for apertures, corners, caves, thin walls, pillars, shaped blocks,
many emitters, moving sun, destruction, streaming, teleportation, negative and
rebased coordinates, distant terrain, water, glass and foliage. Torch removal
must return to a matched no-torch state without stale source energy. Test cache
capacity, bounded queues, both frame slots and non-RT execution.

Slang through standalone Slang RHI remains the only first-party GPU path.
DX12, Vulkan and Metal require separate runtime qualification. Compilation or
Windows captures are not evidence for another platform. Existing FSR 2.2.1
remains the final temporal reconstruction path.

## Implementation status

Reference inspection and owner mapping are complete. Screenshot defaults are
updated in source. The mesh-independent CPU `VoxelTraceWorld` foundation,
bounded `VoxelTraceUpload` GPU residency plan, and renderer-owned
`WorldTracePublication` worker publication are integrated and compile in the
shared native build; the CPU qualification executable passes from
`build/release-windows/tools/native/bin/`. Evidence:
`logs/build/src-foundation-integration3.log` and
`logs/client/voxel-trace-qualification-integrated.log`.

The shared Slang hierarchical DDA query now exists under
`octaryn-client/Shaders/VoxelTracing/`: `VoxelDda.slang` (Amanatides/Woo core
with exact half-open boundary ownership, zero direction components,
simultaneous-crossing X/Y/Z ties and no epsilons), `VoxelQuery.slang`
(chunk 32³ → macro 16³ → leaf 4³ → voxel composition over uint2 masks with a
bounded step budget and distinct miss/unknown/exhausted semantics),
`VoxelTraceBuffers.slang` (GPU contract matching `VoxelTraceUpload::bind`
exactly: `voxelTraceChunks/Leaves/Macros/Materials/Hash`, `voxelTraceCounts`,
`voxelTraceHashMask`, `voxelTraceGeneration`, with `trace_chunk_hash`
replicated bit-identically and linear probing over `entry = index + 1`, zero
empty), and `VoxelSegmentQuery.slang`, which exposes the segment API
`bool voxel_trace_segment(float3 originLocalRelativeToIntegerAnchor, float3
dir, float tMin, float tMax, int3 anchorBlock, out VoxelTraceHit hit)` where
world position is `float(anchorBlock) + anchorRelative`. Hits carry the global
voxel, material id, entry face, `t`, `tExit`, chunk and geometry epoch;
occupancy is a material candidate only, and cutout/fluid/glass classification
stays with callers through atlas material flags. Integer rebasing is exact for
anchors including `(2000000000, -2000000000, 16777217)`; no wave-size
assumptions or hardware ray tracing are used.

Validation is CPU-differential, not GPU runtime: `tools/validation/`
`validate_voxel_dda.py`, `validate_voxel_query.py`, `validate_voxel_buffers.py`,
`validate_voxel_surface.py` and `validate_voxel_segment.py` compile the
production Slang for SPIR-V, DXIL and MSL plus Slang's C++ target, execute it,
and compare against independent Python oracles (including a float64
Amanatides/Woo mirror for segment rays). Reports: `logs/tools/
voxel-dda-boundaries.json`, `voxel-query-boundaries.json`,
`voxel-buffer-contract.json`, `voxel-segment-query.json` and
`logs/tools/voxel-surface.log`, all passing as of 2026-09-17.

Remaining before any SRC claim: actual GPU scene capture of the SRC passes through
the packaged qualifier, plus debug visualizers. Renderer pass integration now
exists but is opt-in: `OCTARYN_CLIENT_GI=src` selects Split Radiance Cascades and
the default remains DDGI until GPU qualification passes.
`RenderBackend/WorldSrcIntegration.cpp` supplies `srcOrigin=floor(eye)` and
`srcEye=eye-srcOrigin`, binds the trace upload plus world atlas through
`SrcFrame::bind_trace`, and `Hdr/CompositeSrc.slang` consumes `srcIrradiance`
(RGB irradiance, alpha donor coverage) with zero indirect where coverage is
absent. `tools/validation/validate_lighting_architecture.py --gi src` drives the
isolated fixture and accepts `--no-rhi-validation`.

Implemented after first-light runs: probe-level 36-bin cosine-weighted
irradiance pre-integration (`Irradiance.slang`, `srcOct`/`srcOctPrev`) replacing
per-pixel directional gathers; fixed-point Q16 integer deposits replacing
compare-exchange float loops (the device reports no float atomics); and a
previous-frame fence wait before SRC dispatch because the scratch cache is
single-instance while two frames are in flight.

### Freeze safety — 2026-09-17

Enclosed-fixture SRC runs could hang the GPU long enough to trigger driver
timeouts and freeze the user's desktop. The client now fails safe:
`OCTARYN_CLIENT_FENCE_TIMEOUT_MS` (default 8000) bounds every frame-queue fence
wait, and `OCTARYN_CLIENT_FRAME_WATCHDOG_MS` (default 5000, 0 disables) fails any
frame whose CPU wall time exceeds the budget. Either trip sets a fatal renderer
status (`fence_timeout` / `frame_watchdog`) and the session exits through the
existing single-frame-failure path, so a pathological submission can freeze the
desktop at most once per launch instead of every frame. All fence waits in
`WorldRenderer.cpp`, `WorldCapture.cpp` and `WorldSrcIntegration.cpp` use the
bounded timeout. Qualification runs must keep the defaults; only raise them
deliberately for known-slow first launches, and never in normal play.

### Known open performance defect — 2026-09-17

Open/natural scenes run SRC at 10–12 ms/frame at 960x540 with clean 420–480
frame exits (`logs/client/src-fresh-oct`, first `src-fresh-torch2` success). The
enclosed stone-box lighting fixture instead spends about 1.0–1.3 s/frame inside
the Resolve pass (`src_contact` column of `logs/client/src-fresh-torch2/ser2.csv`),
where every terrain pixel runs the 32-direction contact DDA plus donor
visibility traces against fully resident geometry. Streaming-world runs are fast
only because few pixels are terrain yet. This must be fixed (contact term
restricted to pixels whose C0 donors lack coverage, or a true cascade-1 probe
layer) before SRC can qualify or become the default. First launches after shader
edits also spend minutes compiling the SRC pipeline set; qualification timeouts
must allow for that. The D3D12 debug layer adds second-scale per-frame overhead
to SRC command lists, so SRC qualification currently uses `--no-rhi-validation`;
that limitation is recorded, not resolved.

No SRC capture has passed the packaged qualifier yet, so nothing here claims the
DDGI replacement is complete. The active request in `REQUESTS.md` remains open.
