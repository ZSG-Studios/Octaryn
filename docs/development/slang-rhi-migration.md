# Standalone Slang RHI migration

The requested active renderer is standalone `slang-rhi`, pinned at
`e17f6d75` under `build/dependencies/slang-rhi`. The previous GFX workspace was
preserved before this migration. `gfx::` aliases are not a standalone RHI port.

## Atlas, sky, and UI owner conversion

The existing `Rendering/Atlas`, `Rendering/Sky`, and `Rendering/Ui` owners now use
real `rhi::IDevice`, textures/views, buffers, samplers, shader objects, pipelines,
command encoders, and render/compute passes. Their shader source is unchanged.
Atlas retains 29 layers, six mip levels, three maps, original mip generation,
point cutout filtering, nearest preview filtering, linear anisotropy 8, and
20-tick animation scheduling. Multi-mip animation uploads use the pinned RHI
API's required `Extent3D::kWholeTexture`, followed by submission and completion.
Sky retains the 112-byte uniform block, triangle draw, LessEqual depth comparison,
and disabled depth writes. UI retains its compute overlay and bounded HUD
rectangles, with an explicit barrier between potentially overlapping dispatches.

All six changed implementation units compiled directly against the pinned RHI
header, its actual generated static-library configuration, and the shared shader
helper header. Object compilation is not GPU runtime verification; aggregate and
packaged runtime evidence must be recorded separately after those checks run.

## Retired GFX probes pending port

These ten executables and their validation targets were removed from the active
CMake graph. Their original source remains in place for a later real RHI port.
They are neither passing nor replaced by successful skip targets:

| Retired executable target | Pending coverage |
|---|---|
| `octaryn_client_render_backend_swapchain_probe` | Original isolated swapchain probe |
| `octaryn_client_voxel_upload_probe` | Voxel GPU upload |
| `octaryn_client_voxel_occupancy_probe` | Occupancy compute |
| `octaryn_client_voxel_face_mask_probe` | Face-mask compute |
| `octaryn_client_voxel_greedy_count_probe` | Greedy-count compute |
| `octaryn_client_voxel_prefix_range_probe` | Prefix-range compute |
| `octaryn_client_voxel_packed_quad_emit_probe` | Packed-quad emission |
| `octaryn_client_voxel_indirect_generation_probe` | Indirect-command generation |
| `octaryn_client_voxel_raster_frame_probe` | Original raster-frame fixture |
| `octaryn_client_voxel_world_frame_loop_probe` | Original frame loop and radius-32 fixture |

CPU voxel invariants, mesh, and indirect-reference probes remain active. The
`octaryn_client_render_backend_probe` remains active and now validates an
actual standalone RHI device/submit/readback check. Its validator executes on a
native host, including Windows; cross-compilation cannot produce a fake pass.

## Packaged runtime diagnostic

`octaryn_validate_client_rhi_diagnostic` runs the actual packaged
`Octaryn.Client --diagnostic`, which renders 180 frames after authoritative player
state and terrain arrive. Each invocation creates a fresh isolated world beneath
the build validation directory and retains its log and saves. The validator
requires a successful exit, authoritative player readiness, at least 180 frames,
complete configured column-window residency, positive quad/GPU-byte counts, the
standalone RHI/Vulkan device marker, and no RHI or Vulkan validation errors in its log.
Timeouts and missing counters fail. It supplies no synthetic chunk stream and
does not modify production saves.

This bounded runtime check does not establish full radius-32 coverage, equality
with every retired GPU algorithm probe, or complete original-engine parity.

## Verified cutover, 2026-09-13

The existing interactive world now uses actual `rhi::` device, surface, queue,
command encoder, shader objects, compute/render pipelines, textures and buffers.
The imported dependency is upstream commit
`e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc`, built static with Vulkan enabled.
Slang SDK 2026.17.1 supplies the compiler. The packaged `slang-glslang.dll` supplies
Slang's SPIR-V optimizer; this is not a GLSL shader path. All 40 owned shader
sources remain Slang (23 compile roots, 18 explicit entry points).

Build and evidence:

- `build/slang-rhi-all-build.log`: aggregate build passed.
- `build/slang-rhi-diagnostic-final-build.log`: final bundle and diagnostic passed.
- `build/release-windows/client/validation/rhi-client/run-b27z1rct/client.log`:
  real RHI Vulkan on RX 9070 XT, 180 fully resident frames, 81 columns,
  3,353,335 faces, exit 0. Vulkan Core/Synchronization and RHI validation enabled;
  zero errors or warnings through destruction.
- `logs/client/slang-rhi-backend-probe.log`: actual GPU clear/submit/readback passed.
- `logs/client/slang-rhi-linkage.log`: 42 active PE artifacts, zero GFX imports,
  zero stale GFX DLLs; active Ninja link graph has no gfx.lib.
- `logs/client/slang-rhi-shaders.log`: all shader compilation checks passed.
  Final bundle comparison, native ownership, and 500-line checks passed.
- `logs/client/slang-rhi-verified.bmp`: final GPU color image copied to the RHI
  surface, visually checked for terrain textures, sky, HUD, sprites, glass and lava.
- `logs/client/slang-rhi-verified-geometry.json`: all 3,353,359 fixture GPU faces
  valid across 81 columns; exact torch/flower/glass/water/lava fixture counts pass.

The initial RHI debug layer rejected `kEntireTexture` for copyTexture because its
UINT_MAX counts were invalid for that operation. Explicit one-layer/one-mip
ranges fixed the rejected copy and undefined-layout presentation errors. A
lifetime-safe debug callback now reports RHI errors and causes frame failure;
the packaged diagnostic also fails on shader/RHI/Vulkan warnings and errors.

Session and world-stream source remained byte-identical to the pre-migration
backup (14 files checked). The backup is
`C:/Users/Rose-X/Documents/Octaryn-Backups/2026-09-13-before-slang-rhi`.
Ten obsolete GFX probe executables were hash-verified into its
`retired-gfx-probes` directory before removal from active binary directories.
Their original source remains available for deliberate probe ports.

## Uncapped performance baseline

RX 9070 XT, 1280x720, fixed fixture view, 81 retained columns and 61 drawn columns,
5 seconds warmup plus 15 measured seconds; validation off. These are local
fixed-view measurements, not general gameplay or cross-platform guarantees.

| Run | Mean frame time | Equivalent average FPS | Histogram 1% low | Worst frame |
|---|---:|---:|---:|---:|
| `slang-rhi-benchmark.log` | 1.276 ms | 784 | 400 FPS | 224.078 ms |
| `slang-rhi-benchmark-repeat.log` | 1.090 ms | 917 | 500 FPS | 13.923 ms |

Both logs and adjacent CSVs are under `logs/client`. The first run contains
several severe render-stage stalls; the repeat is cleaner but does not erase
that evidence. Session work remains about 0.001 ms. Render queue waits and
remaining spikes need separate profiling before claiming stable frame pacing.
The backend cutover is verified; performance qualification remains bounded.

## Continue restoration here

Full original HDR/G-buffer compositing, fluid appearance/flow, remaining
lighting/cloud behavior and player-model integration are still unfinished.
Do that work on standalone RHI, preserving the original engine/reference owners.
GPU ray tracing, full radius-32 rendering, Linux/macOS and Windows ARM64 are not
verified. There is no active GFX or GLSL fallback.

Reproduce the native dependency with `tools/build/slang-rhi.ps1`, configure with
`tools/build/windows.ps1 -Action configure`, build with
`tools/build/windows.ps1 -Action build`, and run the packaged diagnostic target
`octaryn_validate_client_rhi_diagnostic`. The normal client is launched with
`tools/build/windows.ps1 -Action run-client`.
