# Full-detail voxel performance

The September 13, 2026 repair keeps all voxel detail. No LOD or reduced-distance
substitute is introduced. Active GPU work remains Slang on standalone SlangRHI,
with Vulkan selected through RHI. Original reference source is read-only at
`ref/upstream-octaryn`, revision `3557cbfdc803ec034122bb55070b62b3b43b5588`.

## Changes

- `WorldGreedy.slang` merges matching visible cube faces into rectangles within
  bounded 32-by-32 face planes. Full block ID and direction are merge keys.
  Shared row masks drive extraction; there is no subgroup-width assumption.
  The count pass uses one global atomic per occupied plane.
- `WorldFaces.slang` retains count/allocate/emit and the 16-byte face record.
  Bits 20–24 and 25–29 encode width and height minus one; directions use bits
  16–19. The minimum signed voxel anchor and full material ID are retained.
  Cutout leaves, sprites, glass, water and lava remain individual faces. Fluid deformation,
  flow, corner heights and triangle diagonal selection retain their old rules.
- `WorldRaster.slang` expands rectangles and repeats texture coordinates in
  voxel units. Existing array textures, mip derivatives, PBR and per-tile POM
  remain active. Merging changes triangulation, not voxel surfaces.
- `WorldDraw.cpp` restores original near-to-far opaque/sprite order and retains
  far-to-near forward order. It reuses one SlangRHI root object and atlas
  binding per populated pass; RHI snapshots changing column bindings per draw.
- Renderer center updates skip unchanged windows. Cached resident geometry and
  GPU-buffer totals replace whole-column scans on each statistics request.
- Benchmarks wait for both full residency and an empty neighbor-remesh queue
  before warmup. `pending_meshes` is recorded in the existing CPU CSV.

These are exact-surface optimizations. They do not add a production CPU mesher,
change server generation, reduce material features, or alter production saves.

## Profiling

Set `OCTARYN_CLIENT_GPU_PROFILE_PATH` to an output CSV path to enable device
timestamps. Seven intervals cover sky, opaque terrain, HDR composition, forward
rendering (player/clouds/transparency/selection), tonemapping, RmlUi and copy.
All markers are outside render passes. Results are read after the renderer's
existing completion wait; profiling does not introduce another GPU wait.
The same CSV records CPU wall time for neighbor meshing, atlas updates, image
acquisition, draw preparation, encoding, submission, presentation and completion.

GPU totals cover the render submission. They exclude preceding mesh generation
and animated-atlas updates, CPU command preparation, image acquisition and
presentation waits. Compare them with `open-world.csv` CPU timings, rather than
treating GPU duration as total frame latency. Disable profiling for normal use.

## Qualification

Final Windows x64/RX 9070 XT package: 152 payload files and 800 first-party code
files pass the installed-payload, owner/SlangRHI graph and 500-line checks.
CPU checks pass 3,264 halo cases and 137 draw/statistics cases. Headless GPU checks
pass 14 surface fixtures, 22 raster comparisons and two byte-identical multiple-
column binding comparisons. Full cutout-leaf output is identical.

Packaged Vulkan core/synchronization validation passes the 81-column world and
the dedicated fluid world, with 573 RmlUi checks per run. Each logs two existing
`Shader-OutputNotConsumed` warnings and no validation errors; the headless probe
logs ten such warnings. Validation mode now requires the core layer explicitly.
Live Apply passes 4 -> 8 -> 4 with 81 -> 289 -> 81 actual renderer columns and
no OS input injection.

The ordinary capture expands 320,431 rectangles to exactly 1,483,600 unit faces,
matching the prior radius-4 package. The separate fluid capture passes 60 water/
lava faces, all eight levels and all 18 authored cases, with maximum height/flow
error 5.96046448e-8. The ordinary terrain capture contains no fluids and is not
used as fluid evidence. Fluid fixture geometry also passes the rectangle validator.

The independent headless target is `octaryn_validate_client_world_mesh`.
It exercises the production GPU mesher, atlas and raster pipelines, expands
retained rectangles into unit faces, and compares an independent catalog-based
surface oracle. Cases include signed/full-height columns, partial height tiles,
halos, material boundaries, checkerboards, concave surfaces and fluid levels.
The runtime capture validator understands the same packed rectangle format and
rejects invalid extents, overlap, reserved bits and malformed records.

Image comparisons include all four G-buffer targets, depth, six orientations,
near POM, distant mip sampling and alpha cutouts. Nearest-filter texture sampling
requires distinguishing genuine UV errors from tiny triangulation-rounding
differences at a texel or mip boundary. Preserve failed diagnostic evidence;
never hide a broad image mismatch behind a larger blanket tolerance.

Reproduction on the configured Windows checkout:

```powershell
& tools/build/windows.ps1 -Action build -Target octaryn_client_bundle,octaryn_validate_client_draw_preparation
& tools/build/windows.ps1 -Action build -Target octaryn_validate_client_world_mesh
& work/run-no-lod.ps1 -Tag no-lod-after-32 -Distance 32 -GpuProfile
```

Baseline package: `build/no-lod-baseline-20260913/client/bundle`, a complete
copy made before replacing the installed package. Do not mix its DLLs or shaders
with current outputs. Benchmark worlds are isolated under build directories.

## Measurements

Same isolated generation-revision-2 world/seed, stationary right-shoulder view,
1280x720, F3 visible and uncapped presentation. Tables use the final approximately
nine seconds of one-second CSV reports: elapsed time divided by completed frames,
not an average of instantaneous FPS. Column/quad counts and camera pose are stable.
Animation remains active. These are stationary samples, not fast-travel tests.

| Distance | Columns | Before frame / FPS | After frame / FPS | Before / after quads |
|---|---:|---:|---:|---:|
| 4 | 81 | 0.8935 ms / 1,119 | 0.6057 ms / 1,651 | 1,483,600 / 320,431 |
| 32 | 4,225 | 12.4260 ms / 80.48 | 7.1585 ms / 139.69 | 59,854,304 / 15,751,869 |

At distance 32, tracked mesh/frame GPU allocation estimates fall from
1,017,454,448 to 311,815,488 bytes (69.4%). This is not total device VRAM;
atlas/player/swapchain/driver allocations and transient resources are excluded.
The final sample draws the same 1,739 columns. Geometry falls 73.7%; its CPU/GPU
profiling was enabled, while baseline profiling used the existing CPU CSV only.
Radius-4 before/after both run without optional device profiling.

The first greedy run measured 8.3395 ms/119.91 FPS. Reusing the per-pass RHI root
reduced the final observed sample to 7.1585 ms/139.69 FPS. GPU time stays about
1.69 ms. Final CPU timing attributes 4.71 ms to command encoding, 0.14 ms to draw
preparation, 0.28 ms to submission, 0.07 ms to presentation and 1.65 ms to GPU
completion. Opaque terrain accounts for 1.45 ms of GPU time; forward rendering
0.15 ms and RmlUi 0.018 ms. This makes further submission batching the priority.

Evidence: `logs/client/no-lod-comparison-4.json`, `no-lod-comparison-32.json`,
the corresponding before/final CSVs, and `no-lod-final-32-gpu.csv`.
The long radius-32 baseline has a stable captured window but lacks a normal exit
record, so its timing comparison is provisional. A repeat ended at 4,197 columns
before residency and is excluded. Both final new-package benchmarks and the
radius-4 baseline exit normally. Earlier short radius-32 numbers included pending
neighbor rebuilds and are not steady-state baseline measurements.

The final radius-32 benchmark reports 62.5 FPS 1% low and a 33.762 ms worst frame
after warmup. Loading still produced a 2.359-second outlier and two movement-buffer
underruns; neither occurred in the final comparison window. This is a substantial
steady-view improvement, not a claim that streaming or movement hitching is solved.

## Next measured priorities

1. Batch column submission through shared GPU storage and indirect arguments.
   Geometry reduction does not remove per-column descriptor and draw costs.
   Preserve transparent ordering and test independent column bindings.
2. Make count/readback/emit asynchronous with bounded pending work. Retain
   immutable voxel/halo inputs and publish only the matching completed revision;
   edits and eviction must never expose stale geometry.
3. Improve cooperative mask loading and separate special faces from the kernel
   that reserves greedy shared memory. Benchmark checkerboards as well as solid
   terrain; homogeneous fixtures alone exaggerate meshing gains.
4. Evaluate conservative visibility for full-detail tiles, including hidden-face
   directions and hierarchical depth rejection. Keep disocclusion and near-plane
   cases correct; no missing geometry or delayed appearance is acceptable.

These follow-ups are not implemented by this change. Remote multiplayer,
other desktop GPUs/OSes, sustained fast travel and complete engine feature parity
remain separate qualification work.

## Algorithm references

The original engine emitted unit faces but already represented face spans in its
old voxel packing. The current implementation keeps Octaryn material/visibility
rules and uses bounded GPU row masks and rectangle extraction. Useful primary
algorithm references are [binary greedy meshing](https://github.com/cgerikj/binary-greedy-meshing)
and [0fps greedy meshing](https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/).
Material and normal boundaries follow the constraints discussed in
[meshing part two](https://0fps.net/2012/07/07/meshing-minecraft-part-2/).
Texture repetition and derivatives are checked against the concerns in
[texture wrapping and mip mapping](https://0fps.net/2013/07/09/texture-atlases-wrapping-and-mip-mapping/).

## Follow-up: prioritize visible boundary repairs

The pending halo queue previously chose the smallest column coordinate. A nearby
edited boundary could therefore wait behind distant initial-loading work, leaving
its neighbor mesh temporarily stale. `WorldMeshInvalidation.cpp` now promotes
changed boundaries of an existing source into an urgent set, including neighbors
already queued by a prior arrival. New arrivals remain ordinary work.
`WorldMeshHalo.cpp::world_mesh_take_pending` chooses the nearest urgent column,
then the nearest ordinary column, with deterministic coordinate tie ordering.
All unselected work remains queued; successful replacement and eviction clear
both pending sets. Rebuilding remains limited to one halo mesh per rendered
frame, with no added GPU wait, geometry reduction, or authority change.

The production selector scans at most the retained 4,225-column window. It uses
64-bit coordinate differences, removes orphaned priority entries, and takes an
empty-queue path without scanning columns. The focused maximum-size CPU fixture
measured **134.797 microseconds mean** across 64 selections. This is not a measured
worst case, frame-time reduction, or GPU performance result.

`octaryn_validate_client_draw_preparation` passes in
`build/voxel-culling-after.log`: 3,343 halo/scheduling checks, 12,006 camera checks,
and 137 draw checks. Scheduling regressions in
`tools/Source/ClientDrawPreparationProbe/HaloInvalidationProbe.cpp` exercise
already-pending edit promotion, nearer new-arrival work, urgent proximity,
deterministic ties, replacement/unload cleanup, and preservation of the full
maximum-size pending set. These are CPU checks against production logic;
this follow-up has no new GPU or live-game qualification yet.
