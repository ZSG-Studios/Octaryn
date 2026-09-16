# Exact voxel multi-draw batching

Status: implemented and qualified for graphics correctness on Windows x64,
Vulkan, AMD Radeon RX 9070 XT. The controlled local comparison below also
measures reduced frame and CPU submission time.
This change preserves full voxel detail; it introduces no LOD, mesh inflation,
CPU mesh fallback, or client world authority.

## Submission and shader contract

`octaryn-client/Source/Rendering/RenderBackend/WorldBatch.h` and `WorldBatch.cpp`
collect the existing visible-column list into two pass ranges: opaque and crossed
sprites. Both retain the original near-to-far order, culling, depth tests and
material shading. `WorldDraw.cpp` leaves glass, lava and water on the existing
far-to-near per-column path, with unchanged blend and depth behavior.

Each batch record is 24 bytes: two 64-bit descriptor handles for a column's face
and patch buffers, its pass patch offset, and padding. Each indirect argument is
16 bytes: six vertices, the existing patch count, zero first vertex, and the
record index as first instance. `WorldBatchRaster.slang` uses `SV_InstanceID` for
the local patch instance and `SV_StartInstanceLocation` for the batch record.
The sprite pass therefore preserves both its nonzero patch offset and the
record index independently. Commands split at the device's advertised maximum
indirect draw count without resetting either index.

The existing 16-byte face records, packed patch mapping and per-column
160-byte argument buffers are unchanged. The batch shader shares the actual
vertex-coordinate and material functions in `WorldRaster.slang`; it does not
reconstruct a different mesh. The existing watertight patch topology remains
active in both submission paths.

## Capabilities, bounds and fallback

The dependency remains pinned to standalone Slang RHI commit
`e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc`. The narrowly scoped
`tools/build/patches/slang-rhi-multi-draw-capabilities.patch` exposes
`MultiDrawIndirect`, `DrawIndirectFirstInstance`, `ShaderDrawParameters` and
`DeviceLimits::maxDrawIndirectCount`. The Vulkan feature reports follow the
features supplied to device creation. Engine code consumes the RHI API; it does
not query or submit raw Vulkan commands. Imported externally created devices
are not part of this qualification.

`tools/build/slang-rhi.py` checks and applies this exact patch alongside
the existing `slang-rhi-descriptor-capacity.patch`, rejecting unexpected tracked
changes. The descriptor-capacity patch remains intact.

The batch is bounded to 4,225 visible columns, 8,450 pass records and 16,384
buffer descriptors. Initialization requires Bindless plus all three draw
features and an indirect draw limit of at least two. Descriptor handle zero is
valid; validity uses the handle type. There is no unbounded geometry arena.

`OCTARYN_CLIENT_WORLD_BATCH` accepts `auto` (default), `off`, or `required`.
Unavailable capabilities, resources or capacity retain the legacy path in auto
mode; required mode fails rather than pretending to qualify batching. If device
creation rejects the expanded descriptor capacity, initialization retries the
original capacity and disables batching. A renderer-owned initial-device debug
callback separates diagnostics from that failed attempt. Errors belonging to a
successfully created device remain fatal; they are never cleared by fallback.
This fallback is implemented and reviewed, but rejection on a lower-capability
physical device has not been exercised by the RX 9070 XT qualification.

## Resource lifetime and accounting

Preparation runs after visibility selection and before the first render pass.
It explicitly transitions all face and patch buffers referenced through stored
handles, since RHI binding reflection cannot discover those resources. It also
sets the batch-record and indirect-argument states. Retained buffer references
keep descriptor slots alive until the previous submitted frame has completed.
The existing end-of-frame queue wait precedes reuse of the mapped upload
buffers. This change adds no frame-in-flight ring or new per-frame GPU wait.

The fixed uploads add **338,000 tracked bytes**:
`8,450 * (24-byte record + 16-byte argument)`. `WorldRendererStats.gpu_bytes`
counts the actual allocated batch buffers, including partially initialized
buffers retained after fallback. Driver descriptor-pool overhead is not included
in that buffer accounting. Command counts and the active batch mode are appended
to the existing opt-in GPU CSV with viewport dimensions. The separate periodic
batch printf/flush was removed after profiling found large renderer-tail stalls
coincident with those reports. No recurring batch stdout output remains.
All added first-party C++/Slang source files remain below the 500-line limit.

## Verified evidence

`build/mip-batch-parity-first.log` records the real headless production path:

- Six view/shading combinations compare all four G-buffers byte-for-byte and
  depth exactly against legacy submission, using two distinct material columns.
- Both columns have multiple opaque and sprite instances. Nonzero record indices
  and sprite patch offsets are exercised: four legacy commands become two actual
  multi-draw commands. Forcing a one-draw limit produces four split commands with
  identical output.
- Replacement, eviction and an empty draw list do not retain stale submissions.
- All 256 moving seam views run with batching forced: 2,288,695 interior pixels
  have zero holes, and 1,248,704 independent analytic floor samples have zero
  missing pixels. The fixture contains 399 merged faces versus 21,888 unit faces,
  with no geometry inflation.

The reproducible explicit hardware target is
`octaryn_validate_client_world_batch` in
`cmake/Owners/ToolTargets/ToolWorldMeshProbe.cmake`. It depends on the canonical
world-mesh validator for build, staging and ordinary graphics checks, then runs
the staged probe with `--batch-only` under the same validation environment.
Unsupported batch hardware fails this explicit qualification. Ordinary mesh
shader-diagnostic cases remain on the legacy path, so batching cannot bypass
their deliberately substituted diagnostic pipelines.

The packaged radius-four run is recorded in
`logs/client/mip-batch-validated-4.log` and `mip-batch-validated-4-error.log`.
It reports standalone RHI/Vulkan, the RX 9070 XT, bindless mode, all 81 columns
resident, mesh completion and normal `open_world_exit code=0`. Steady submission
records show 64 visible opaque columns in one command; forward rendering remains
legacy. Core and synchronization validation were active, with no logged RHI
errors or Vulkan validation errors. Existing unused shader-output warnings remain.
The matching `.bmp`, `.bmp.quads.bin`, `.bmp.fluids.bin`, frame `.csv` and
`-gpu.csv` retain the capture, exact mesh records and timings for inspection.

## Final local performance comparison

`logs/client/mip-final-{off,on}-32` contains completed runs of the same installed
binary, with batching explicitly off or required. Both use 1280x720, render
distance 32, the same camera and world, full 4,225-column residency, no pending
meshes and a five-second settled warmup. Validation is disabled for these timing
runs. The final rolling measurement windows contain 376 and 655 frames;
all outliers remain included. No other Octaryn client was running.

| Metric | Individual draws | Batched draws |
|---|---:|---:|
| Mean frame time | 13.946 ms | 7.832 ms |
| FPS derived from mean | 71.70 | 127.68 |
| CPU prepare + encode + submit | 7.787 ms | 2.627 ms |
| Mean GPU time | 5.488 ms | 4.873 ms |
| Worst measured frame | 60.501 ms | 17.977 ms |
| Histogram 1% low FPS | 32.26 | 71.43 |
| Opaque multi-draw API calls | 1,739 | 1 |

Mean frame time decreased 43.84%; combined CPU preparation, encoding and
submission decreased 66.27%. These are short sequential local measurements,
not universal performance guarantees. Preparation is included because batching
moves work out of encoding. The per-frame CSV confirms mode, command count,
1,739 visible columns and resolution throughout both measured windows.
Forward passes retain their original per-column ordering.

Both captures contain 15,751,869 compact face records and 46,397,392 patch
instances. The full new capture independently passes duplicate/overlap/surface
validation, expanding to 59,854,304 unit faces; see
`logs/client/mip-final-on-32-parity.json`. Tracked storage changes from
497,743,056 to 498,081,056 bytes, exactly the 338,000-byte batch allocation.
This metric excludes driver-private allocations and is not total VRAM usage.

Final shader/mesh/CPU checks pass in `build/mip-batch-final-validation.log`.
The packaged fluid run `logs/client/mip-final-fluid` passes core/synchronization
validation, RmlUi validation and normal exit. Its independent fluid capture
checks cover all 18 fixture cases, water/lava levels 0-7 and 60 fluid faces,
with maximum height/flow error 5.96e-8. Existing unused-varying warnings remain.
Package/source equality, ownership, render graph, module/server payloads and
the 500-line limit pass in `logs/client/mip-batch-qualification.log`
(826 code files, 157 package files). Qualification reads the manifest from the
actual packaged basegame assembly; an older scratch manifest was not reused.

Installed client SHA256:
`673BB91F22BED642A702278804DCC66AB0EC200E4B47C31CB84FEE9528801D29`.
The structured timing comparison is `work/mip-final-comparison.json`.

## Next slice

Synchronous meshing dispatch/readback waits remain. Bounded asynchronous meshing
is a separate next slice that must preserve source revision, halo dependencies,
query-to-visible delivery coherence and resource lifetime. This batching change
does not claim to remove streaming stalls or complete that work.
