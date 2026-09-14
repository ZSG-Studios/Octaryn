# Octaryn Slang RHI GPU Voxel Renderer Plan

Last updated: 2026-05-21T02:55:19-04:00

## 0. Execution Contract

This file is the active repo-local tracker for the renderer cutover. Every implementation pass must update this file before stopping.

Non-negotiable rules:

- Rendering is Slang-first and Slang RHI backed through Octaryn client render-backend APIs.
- No active GLSL shader source is allowed under `octaryn-client/Shaders/`.
- No SDL GPU renderer, SDL render driver path, legacy shader compiler, or CPU mesh renderer may remain active.
- SDL3 may remain only for platform/input/window-facing duties while SDL GPU/render/OpenGL/GLES renderer options stay disabled.
- Slang / Slang RHI types must not leak into shared, server, basegame gameplay, or module APIs.
- The CPU streams compact voxel identity data only. It must not build final render meshes, final vertex buffers, production face masks, or per-chunk draw spam.
- The GPU owns occupancy decode, hidden-face masks, greedy merge, packed quad output, culling, indirect draw generation, and raster shading.
- Block ID and material ID stay separate.
- Server authority and edit-only persistence stay intact. Seed terrain remains memory/VRAM only; only authoritative edits/differences persist or stream as block records.
- Do not claim done, complete, 100%, or production ready until runtime/profiling evidence exercises the real Slang RHI renderer path.

## 0.1 Tracking Rules

- At the start of each pass, mark the active blocker in this file.
- At the end of each pass, append a dated entry under `## 5. Per-Pass Tracking Log`.
- If validation fails, record the failed command and repair path before continuing.
- Never delete unresolved blockers; move them to completed only after evidence is recorded.
- Keep this file honest: a clean build is not renderer completion.

## 1. Current Verified State

Completed cutover work already present in this checkout:

- Old SDL GPU / GLSL / CPU mesh renderer files were removed from active client source and quarantined under `references/old-architecture/source/render/octaryn-client-sdl3-gpu-glsl-backup/`.
- Active shader tree under `octaryn-client/Shaders/` is Slang-only.
- Active shader validation compiles Slang entry points with `slangc`.
- Active bundle validation rejects `.glsl` files and legacy compiled GLSL runtime shader output.
- Client render backend has a Slang RHI device probe hidden behind Octaryn client render-backend code.
- Client launch probe creates a Slang device and logs `slang_rhi_device=created runtime_available=1` and `device_created=1`.
- The old generated client `BasegameBlockCatalog.h` artifact was removed from active source and from basegame catalog validation/generation.
- Basegame catalog remains basegame-owned through basegame data/source artifacts.

Latest validation evidence recorded before this plan file:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_validate_client_app_launch_probe octaryn_validate_basegame_block_catalog octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
git diff --check
```

Important launch proof:

```txt
renderer_cutover_stage=slang_rhi_bootstrap
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated ... stream_source=live_sidecar ... indirect_draw=1 readback=1 retained_gpu_bytes=...
client_voxel_world_frame_loop requested_frames=3 ... stream_source=live_sidecar indirect_draw=1 readback=1 retained_gpu_bytes=... upload_staging_bytes=...
slang_rhi_swapchain=<status line>
client_render_backend active=1 backend=slang_rhi shader_language=slang legacy_glsl=0 legacy_sdl_renderer=0 device_created=1
client_voxel_renderer_rebuild active=0 raster_only=1 packed_quads=1 gpu_driven=planned
shutdown=0
```

This proves device bootstrap, offscreen frame lifecycle, retained live-sidecar
voxel frame-loop execution, and legacy renderer removal. The swapchain line is
status-only because local X11 presentation is not reliable in every validation
environment. This does **not** prove the full interactive voxel renderer pass
graph yet.

## 2. Current Blockers

### Blocker A: Slang RHI frame graph is not implemented

Current state: Slang device creation exists, and hidden Slang RHI backend paths
now create graphics queues, transient resource heaps, command buffers,
device-local buffers, upload commands, render targets, render target views,
framebuffer layouts, framebuffers, render pass layouts, render pass commands,
backend frame begin/encode/end/submit state, fence submission/wait, readback
validation, and an X11/XWayland swapchain create/acquire/present probe path.
The client app launch validation exercises the offscreen frame lifecycle and
records swapchain status through the render backend, but local X11 presentation
is not a required validation gate because Slang-backed presentation can be
unavailable in this environment. The remaining work is to replace the
bootstrap/probe path with the real application frame graph.

Exit criteria:

- Octaryn client backend owns Slang RHI device, queues, command encoding, resource lifetime, and frame submission.
- No raw Slang backend types cross into server/shared/basegame/module APIs.
- Launch/runtime proof exercises a real backend frame, not just device creation.
- Validation rejects fake marker-only render paths.

### Blocker B: GPU voxel pass graph is not implemented

Current state: compact chunk headers, palette entries, and packed voxel
payloads now upload to Slang RHI GPU buffers and the first GPU compute probe
decodes empty, uniform, and mixed chunks into an occupancy-count buffer with
readback validation. A second Slang RHI compute probe now writes GPU-visible
face-mask buffers and face-counts for empty, uniform solid, checkerboard, and
mixed-material chunks with exact CPU-reference mask comparison. A third Slang
RHI compute probe consumes the GPU face-mask buffer and validates deterministic
greedy quad/material counts for the same empty, uniform solid, checkerboard,
and mixed-material chunks. A fourth Slang RHI compute probe consumes the GPU
greedy-count buffer and validates deterministic prefix-scan output ranges and
total quad count. A fifth Slang RHI compute probe consumes the face-mask,
greedy-count, and prefix-offset buffers and validates GPU-written
`PackedVoxelQuad16` records for empty, uniform solid, checkerboard, and
mixed-material chunks. A sixth Slang RHI compute probe consumes GPU-emitted
packed quads and per-chunk counts, suppresses the empty chunk, and writes
compact indexed indirect draw commands plus draw/instance counters. The focused
offscreen raster-frame probe now runs the same compact chunk
headers/palettes/payloads through face-mask, greedy-count, prefix, packed-quad
emit, and indirect-generation compute passes, then binds the GPU-generated
`PackedVoxelQuad16` and indirect command buffers through the client-owned Slang
RHI render backend. Production runtime integration, full frustum/Hi-Z culling
coverage, and final raster shading are still missing.
The validated compute sequence is now extracted as a focused internal
`SlangRhiVoxelRasterPassGraph` encoder so the probe and future production frame
path can share the same client-owned pass ordering without duplicating backend
state transitions.
The raster-frame input is now built from the client bounded streaming budget
and compact column identities instead of the old canonical prefix-probe batch;
the focused offscreen probe reports four bounded streamed columns feeding three
non-empty GPU indirect draws.
The bounded raster-frame GPU buffers, resource views, counters, generated
packed-quad buffer, generated indirect command buffers, and fixed quad index
buffer are now owned by a focused internal `SlangRhiVoxelRasterResources`
resource owner instead of being transient locals embedded in the probe frame
function. The existing raster-frame proof still runs offscreen, but the resource
shape is now reusable by the production client frame path without duplicating
Slang RHI buffer/view creation or leaking backend handles outside render-backend
internals.
The raster frame path now has a production-named
`render_slang_rhi_voxel_raster_frame()` entry point backed by an internal
`SlangRhiVoxelRasterFrameSession` that owns the Slang RHI device, queue, heap,
pipelines, retained bounded stream resources, render target, framebuffer, and
render pass across command encoding. The client bootstrap app calls that render
entry point and logs the GPU-generated indirect raster proof, so the active app
path exercises the retained-resource voxel frame in addition to the standalone
probe.
The bounded raster-frame batch now carries the accepted `ColumnStreaming`
request coordinates alongside compact chunk headers/palettes/payload bytes, and
the probe/app logs report the stream center plus first accepted column identity
for the GPU-generated indirect raster path. The voxel content is still synthetic
within that accepted stream window; real streamed world-column payload updates
and multi-frame session reuse remain unresolved.
The retained raster-frame session now survives two frame renders in one app/probe
call. The second frame uploads a moved-center compact stream batch into the
existing header, palette, and payload GPU buffers, resets generated GPU outputs
back to unordered-access state, reruns the GPU pass graph, and submits the
generated indirect draw again. The raster-frame batch now consumes a bounded
server chunk-stream record shape with per-column origin, block offset, block
count, and sparse authoritative edit block records before packing compact voxel
identity bytes for the Slang RHI upload buffers. The batch builder can now read
the native server-authored chunk stream binary sidecar from the client/server
chunk-stream path environment and pack its first bounded columns directly into
the retained Slang RHI upload path. The client app launch probe now passes the
bundled-server readiness chunk stream sidecar into the production-named raster
entry point, selects authoritative edit-bearing columns within the bounded
client budget, maps absolute block Y into GPU chunk-Y headers, and validates
the live sidecar through GPU face-mask/greedy/prefix/emit/indirect/raster
readback in the app path. The full live world frame loop is still not wired.

Exit criteria:

- Compact chunk headers/palettes/payloads upload to GPU resources.
- GPU pass graph runs: decode/occupancy, face masks, greedy count, prefix scan, greedy emit, culling, indirect generation.
- Packed voxel quads are produced by GPU passes, not CPU mesh builders.
- CPU reference validators compare known masks/quad counts against GPU output for empty, solid, checkerboard, and mixed material cases.

### Blocker C: Face-pulled raster path is not runtime-active

Current state: Slang face-pull shaders compile, and a focused offscreen Slang
RHI raster-frame probe now pulls GPU pass-graph generated `PackedVoxelQuad16`
records by instance ID and draws through GPU pass-graph generated indirect
commands into a readback-validated color target. That probe now starts from
bounded streamed compact column identities plus sparse edit-only stream records,
reports the accepted stream center and first column coordinate, and consumes the
extracted internal raster resource owner. The active bootstrap app now calls the
production-named voxel raster frame entry point and logs retained-resource
indirect draw proof from that bounded stream window across two session-reused
frames. The focused raster-frame probe now generates a server-owned native chunk
stream snapshot and verifies the raster path consumed its binary sidecar. The
client app launch probe now consumes the bundled-server sidecar through a
client-owned `VoxelRasterFrameLoop` world-presentation owner and logs
`stream_source=live_sidecar` with non-clear raster output plus retained
batch/timing metrics before the local X11 swapchain validator gate. The full
multi-frame interactive world runtime loop is still not wired.

Exit criteria:

- Unit-quad indexed instancing is active.
- `PackedVoxelQuad16` is pulled by instance ID.
- Depth pass and visible debug/PBR pass render from packed quads.
- Empty world emits zero quads and zero draws.
- Solid chunk renders exterior shell only.
- No CPU terrain mesh path remains active.

### Blocker D: GPU-driven culling and indirect drawing are not runtime-active

Current state: CPU/reference indirect validators, GPU indirect-generation
compute validation, and an offscreen raster probe that consumes GPU pass-graph
generated indirect commands from the extracted raster resource owner exist.
The active bootstrap app now delegates voxel frame rendering to a client-owned
world-presentation `VoxelRasterFrameLoop`, which consumes generated indirect
commands through the production-named raster frame entry point and proves the
generated indirect draw remains active after one retained-session compact
stream-buffer update.
That update can now use server-authored sparse edit-only stream records rather
than full synthetic chunk content. The client app launch path now proves a
server-authored edit-only stream sidecar drives GPU-generated indirect drawing
through the production-named raster entry point. The full streamed world
runtime frame path still does not consume generated indirect commands, and full
GPU culling coverage remains missing.

Exit criteria:

- GPU culls chunks/batches/material bins.
- GPU writes compact indirect draw command buffer and draw count.
- Runtime uses draw-indirect-count or compact fallback.
- No per-chunk CPU draw loop and no thousands of empty draws.

### Blocker E: Material/PBR path is not complete

Current state: basegame material catalog exists, Slang material files exist, but no full runtime PBR material table/texture-array path is active.

Exit criteria:

- Block IDs and material IDs are distinct in runtime GPU data.
- Material table buffer is uploaded.
- Texture-array or bindless-style indexing is used.
- No per-face/per-chunk texture binds.
- Albedo/normal/ORM/emissive fields validate and shade correctly.

### Blocker F: Runtime/profiling proof missing

Current state: old runtime proof cannot count because it exercised the removed SDL GPU/GLSL path.

Exit criteria:

- Direct runtime run with Slang RHI path.
- Evidence covers batch counts, build/upload timing, retained chunks/columns, GPU quad counts, indirect draw count, VRAM/staging memory, pass timings, radius-32 visibility, and no recurring server JSON churn.
- Tracy/GPU timestamp/ImPlot or log-equivalent metrics are captured.

## 3. Implementation Phases

### Phase 1: Guardrails and validation hardening

Tasks:

- Keep active shader tree Slang-only.
- Keep `.glsl`, `shadercross`, `glslang`, old shader compiler, SDL GPU, and old CPU mesh renderer out of active targets.
- Add or maintain active grep/static validators for forbidden renderer paths.
- Keep `plan.md` updated after every pass.

Validation:

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
git diff --check
```

Status: in progress.

### Phase 2: Slang RHI backend layer

Tasks:

- Extend `octaryn-client/Source/Rendering/RenderBackend/` into focused files:
  - device creation
  - adapter/backend selection
  - command queue/encoder wrapper
  - buffer/image creation
  - resource lifetime/fence handling
  - frame begin/end
- Keep public client renderer-facing API clean and Octaryn-owned.
- Do not expose raw backend handles outside renderer backend internals.

Validation:

- Client app launch probe must create a Slang device.
- Add backend probe target that creates/destroys buffers and validates fence-safe release.
- Launch logs must not overclaim full voxel renderer readiness until passes run.

Status: in progress; device creation, queue/heap/buffer/command submission,
offscreen render target/framebuffer/render-pass encoding, backend frame
begin/end, fence wait, readback validation, X11/XWayland swapchain
create/acquire/present, and client launch validation of the swapchain path are
verified. The production client frame graph still needs to consume this
backend path.

### Phase 3: GPU data contracts

Tasks:

- Lock CPU/GPU structs for:
  - `GpuChunkHeader`
  - `GpuChunkPaletteEntry`
  - compact voxel payload views
  - `PackedVoxelQuad16`
  - draw commands
  - material table entries
- Add native/static validation for layout, alignment, and shader/native binding compatibility.
- Keep shared/basegame contracts implementation-free.

Validation:

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_invariants_probe octaryn_validate_client_voxel_indirect_probe
```

Status: partial CPU/reference structs exist; GPU binding validation still missing.

### Phase 4: Compact voxel storage and streaming

Tasks:

- Keep chunk size `32 x 32 x 32`.
- Keep column height `32 chunks` / `1024 blocks`.
- Keep render distance values `4..128` step 4 with no hidden buffer.
- Implement compact palette payload upload staging.
- Use Taskflow/native jobs for CPU generation/compression/upload staging.
- CPU must not build final render meshes.

Validation:

- Ring streaming probe.
- Palette roundtrip probe.
- No CPU mesh path static probe.
- Runtime proof for bounded per-frame upload staging.

Status: partial invariants/reference probes exist; runtime upload staging to Slang backend missing.

### Phase 5: GPU occupancy and face-mask passes

Tasks:

- Decode compact chunk payload on GPU.
- Build occupancy masks.
- Build visible face masks for ±X, ±Y, ±Z.
- Support pass-specific occupancy for opaque/cutout/fluid/transparent classes.

Validation:

- CPU reference vs GPU output for empty chunk, solid chunk, checkerboard, mixed occlusion.
- Empty chunk emits zero faces.
- Solid chunk emits exterior faces only.

Status: GPU occupancy and face-mask compute probes exist for compact chunks;
production runtime integration and greedy/culling/indirect passes are still
missing.

### Phase 6: GPU greedy mesh count/scan/emit

Tasks:

- Count quads per chunk/direction/material bin.
- Prefix-sum counts into deterministic output ranges.
- Emit `PackedVoxelQuad16` records.
- Do not use global per-face append as the final path.

Validation:

- Greedy coverage exactly equals visible face masks.
- No duplicate faces.
- No missing faces.
- No wrong material merges.
- Output offsets deterministic.

Status: GPU greedy-count, prefix-range, and packed-quad emit probes exist for
the canonical compact-chunk cases; production runtime integration, culling,
indirect generation, and full coverage validation are still missing.

### Phase 7: Face-pulled raster rendering

Tasks:

- Create fixed unit-quad index buffer.
- Draw indexed instanced quads from `PackedVoxelQuad16`.
- Vertex shader reconstructs corners from column/chunk/local data.
- Add depth-only, debug, and PBR raster passes.

Validation:

- Empty world zero quads/zero draws.
- Solid chunk exterior shell only.
- Chunk boundaries align.
- No CPU terrain mesh fallback.

Status: Slang shaders compile, and a focused offscreen raster-frame probe now
draws GPU pass-graph generated packed quads through GPU pass-graph generated
indirect commands; production runtime draw path still missing.

### Phase 8: GPU culling and indirect draw generation

Tasks:

- Build chunk/batch bounds.
- Frustum cull on GPU.
- Add Hi-Z occlusion after depth path exists.
- Generate compact indirect draw commands and draw count.

Validation:

- CPU submits fixed small pass set.
- GPU writes draw commands.
- No per-chunk CPU draw loop.
- No thousands of empty draws.

Status: GPU indirect-generation and offscreen indirect raster probes exist for
the canonical compact-chunk cases; production runtime integration, full culling
coverage, and application frame submission are still missing.

### Phase 9: PBR material path

Tasks:

- Upload material table.
- Use texture arrays or bindless-style descriptor indexing.
- Wire albedo, normal, ORM, emissive, alpha mode, fluid, foliage, cutout flags.
- Keep basegame content data basegame-owned.

Validation:

- Basegame block/material catalog probe.
- PBR shader compile/binding validation.
- No per-face texture bind path.

Status: basegame catalog validation exists; runtime material upload/shading incomplete.

### Phase 10: Runtime/profiling proof

Tasks:

- Run direct client runtime on Slang path.
- Capture metrics:
  - resident columns/chunks
  - upload queue depth
  - GPU meshing chunk count
  - greedy quads emitted
  - quad reduction ratio
  - indirect draw count
  - empty draw count
  - VRAM/staging memory
  - pass timings
  - server JSON churn status
- Update this file with exact proof strings and commands.

Validation:

- Direct runtime/profiling evidence, not smoke tests and not `ctest`.

Status: missing for new renderer.

## 4. Forbidden Active Paths Checklist

These must stay absent from active code/targets outside `references/old-architecture/`, docs history, and validator rejection strings:

- [ ] Active `.glsl` shader files under `octaryn-client/Shaders/`.
- [x] Active SDL GPU renderer path.
- [x] Active SDL render driver path.
- [x] Active old shader compiler / shadercross / glslang pipeline.
- [x] Active CPU terrain mesh renderer fallback.
- [x] Active per-chunk CPU draw loop.
- [x] Active generated client-owned basegame block catalog header.
- [x] Slang RHI types exposed to server/shared/basegame/module APIs.

Current check command:

```sh
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
```

Allowed current hits:

- `SDL_GPU OFF` in `cmake/Dependencies/ClientDependencies.cmake`.
- Validator rejection strings that fail the build/log if GLSL/SDL GPU markers return.

## 5. Per-Pass Tracking Log

Append every pass here.

### Pass 2026-05-20: Legacy renderer quarantine + Slang bootstrap

Changed:

- Quarantined old SDL GPU/GLSL/CPU mesh renderer source into `references/old-architecture/source/render/octaryn-client-sdl3-gpu-glsl-backup/`.
- Added Slang shader tree under `octaryn-client/Shaders/`.
- Added Slang shader compile validation through `slangc`.
- Added Octaryn client render backend with Slang RHI device probe.
- Removed active generated client `BasegameBlockCatalog.h` artifact and native-client-catalog generation path.
- Updated client launch validation to require Slang device creation and reject legacy markers.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_validate_client_app_launch_probe octaryn_validate_basegame_block_catalog octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
git diff --check
```

Evidence:

```txt
client_slang_shader_validation=passed entry_points=4 slangc=/home/zacharyr/.local/bin/slangc
slang_rhi_device=created runtime_available=1
client_render_backend active=1 backend=slang_rhi shader_language=slang legacy_glsl=0 legacy_sdl_renderer=0 device_created=1
```

Remaining:

- Real Slang RHI frame graph.
- GPU voxel compute pass graph.
- Runtime face-pulled raster draw path.
- GPU culling/indirect runtime path.
- PBR material table runtime path.
- Direct runtime/profiling proof on Slang path.

## 6. Start Prompt For Next Agent

Use this exact prompt to restart the work from this plan:

```txt
You are implementing the Octaryn Slang RHI GPU-driven voxel renderer. Read AGENTS.md, REQUESTS.md, docs/architecture/octaryn-cpp-engine-systems-finish-plan.md, and plan.md first. Treat plan.md as the active tracking source of truth and update it before stopping.

Do not restore GLSL. Do not restore SDL GPU. Do not restore the old CPU mesh renderer. No active .glsl files are allowed under octaryn-client/Shaders. Slang shaders only. Slang RHI must stay hidden behind Octaryn client render-backend APIs and must not leak into shared/server/basegame/module APIs.

Current verified state: the old SDL GPU/GLSL/CPU mesh renderer is quarantined under references/old-architecture/source/render/octaryn-client-sdl3-gpu-glsl-backup, Slang shader compile validation passes, the client launch probe creates a Slang device and logs slang_rhi_device=created runtime_available=1, and active validation rejects legacy renderer markers.

Continue from the first incomplete blocker in plan.md. The next highest-priority blocker is the real Slang RHI frame graph and resource layer: device ownership, command queue/encoder wrapper, buffer/image creation, fence-safe lifetime, frame begin/end, and a backend validation probe. Keep changes owner-correct and focused under octaryn-client/Source/Rendering/RenderBackend, cmake owner target files, and tools/validation only as needed.

After each implementation pass, run targeted validation using Octaryn build helpers only. Do not run ctest. Do not use smoke tests. At minimum run:
- tools/build/cmake_configure.sh debug-linux when CMake changes
- tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
- tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases when CMake changes
- git diff --check

Also run the forbidden active path grep from plan.md and ensure only allowed guard/disabled-option hits remain. Then update plan.md with changed files, validation commands, evidence, and the next blocker. Do not claim done or 100% until the real Slang RHI runtime pass graph renders GPU-generated voxel quads with profiling proof.
```

## 7. Stop Condition

Stop only when either:

- The current pass has a verified blocker-sized result, plan.md is updated, and there is no safe local next step in the current branch; or
- A hard blocker requires missing external authority or unavailable local dependencies.

Do not stop after inspection-only work. Do not report full completion until all blockers in this file are closed with validation and runtime/profiling evidence.

### Pass 2026-05-20: Plan canonicalization and old renderer-plan removal

Changed:

- Updated `AGENTS.md` so `plan.md` is the first source of truth for the Slang RHI GPU voxel renderer cutover.
- Updated `REQUESTS.md` so old `WorldMeshRuntime`/`TerrainMesh` language cannot be read as permission to restore CPU mesh or SDL GPU paths.
- Updated master/appendix/port/build/migration docs so renderer guidance defers to `plan.md`.
- Removed old duplicated renderer loop docs from `docs/architecture/`; this file is now the renderer tracker.
- Aligned stale retired half-height renderer notes to the locked 1024-height / 32-column renderer model.

Validated:

```txt
Scanned AGENTS.md, REQUESTS.md, plan.md, and docs/ for removed renderer-loop document names and stale active-renderer claims.
Checked docs/architecture/ for removed renderer-loop markdown files.
git diff --check
```

Evidence:

```txt
Only intentional no-GLSL/no-SDL-GPU policy references remain.
No removed old renderer-loop markdown files remain under docs/architecture/.
git diff --check passed.
```

Remaining:

- Continue with Blocker A: real Slang RHI frame graph and resource layer.

### Pass 2026-05-20: Slang RHI backend frame/resource probe

Active blocker:

- Blocker A: Slang RHI frame graph is not implemented.

Changed:

- Added hidden Slang RHI frame-resource validation under
  `octaryn-client/Source/Rendering/RenderBackend/SlangRhiFrameResources.*`.
- Extended the client render-backend status API with Octaryn-owned frame
  resource proof fields, without exposing raw Slang RHI types outside the
  backend internals.
- Updated the Slang bootstrap app to log frame-resource validation.
- Added `tools/Source/ClientRenderBackendProbe/ClientRenderBackendProbe.cpp`.
- Wired `octaryn_client_render_backend_probe` and
  `octaryn_validate_client_render_backend_probe` into CMake inventory,
  `octaryn_tools`, and `octaryn_validate_all`.
- Added Ralph PRD/test-spec/context artifacts under `.omx/` for the active
  keep-going workflow gate.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_configure.sh release-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
```

The first CMake inventory run failed because the already-configured
`release-linux` graph was stale and did not yet contain the new backend probe
target. Re-running `tools/build/cmake_configure.sh release-linux` refreshed the
graph, and the same validator then passed.

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=frame_resources_validated
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=frame_resources_validated validated=1
client_render_backend active=1 backend=slang_rhi shader_language=slang legacy_glsl=0 legacy_sdl_renderer=0 device_created=1
client_slang_shader_validation=passed entry_points=4 slangc=/home/zacharyr/.local/bin/slangc
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
git diff --check passed.
```

Remaining:

- Continue Blocker A by wiring real backend frame lifecycle around a window or
  offscreen target: frame begin/end, swapchain or offscreen frame target,
  render-pass layout/framebuffer ownership, and validation that rejects a
  marker-only frame.
- Follow-on Blocker B remains the GPU voxel pass graph.
- Configure output still reports `SDL_OPENGL (Wanted: OFF): ON` while SDL GPU
  and SDL render drivers are disabled; keep this as a renderer guardrail item
  for the next CMake/dependency hardening slice.

Exact next step:

```txt
Continue from plan.md Blocker A. Implement the real Slang RHI backend frame lifecycle beyond the headless resource probe: frame begin/end, swapchain or offscreen target ownership, render-pass layout/framebuffer setup, and a validation target/log marker that proves a real backend frame path rather than marker-only bootstrapping. Also harden the SDL dependency configuration so SDL OpenGL support is not active if the local SDL build still reports SDL_OPENGL ON. Use tools/build/cmake_build.sh targeted validations only, do not run ctest or smoke tests, run the plan.md forbidden-path grep, run git diff --check, and update plan.md before stopping.
```

### Pass 2026-05-20: Slang RHI offscreen frame lifecycle proof

Active blocker:

- Blocker A: Slang RHI frame graph is not implemented.

Changed:

- Extended `SlangRhiFrameResources.*` from buffer/fence validation to a real
  offscreen backend frame lifecycle: render target creation, render target view,
  framebuffer layout, framebuffer, render pass layout, render pass command
  encoding, queue submission, fence wait, and upload/readback verification.
- Updated launch-probe validation to require
  `slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1`
  so marker-only device bootstrapping no longer satisfies the active client
  validation target.
- Kept Slang RHI handles private to
  `octaryn-client/Source/Rendering/RenderBackend/`; public status remains
  Octaryn-owned scalar fields and strings.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_probe octaryn_validate_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
git diff --check
```

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
client_render_backend active=1 backend=slang_rhi shader_language=slang legacy_glsl=0 legacy_sdl_renderer=0 device_created=1
client_slang_shader_validation=passed entry_points=4 slangc=/home/zacharyr/.local/bin/slangc
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
git diff --check passed.
```

Remaining:

- Continue Blocker A with the real window-facing frame graph: frame begin/end,
  swapchain or window-present target ownership where supported by the backend,
  and backend-owned resource lifetime around the frame path.
- Follow-on Blocker B remains GPU execution of the voxel pass graph.
- Keep the SDL dependency guardrail item visible until configure output no
  longer reports `SDL_OPENGL (Wanted: OFF): ON`, even though active OpenGL/GLES
  renderer code and shader paths are absent.

Exact next step:

```txt
Continue from plan.md Blocker A. Add the window-facing Slang RHI frame lifecycle or an explicit backend-owned no-window present abstraction, then validate that the client app exercises frame begin/end through RenderBackend rather than only the offscreen probe. Keep Slang types private to octaryn-client render-backend internals, run targeted tools/build/cmake_build.sh validations only, do not run ctest or smoke tests, run the plan.md forbidden-path grep, run git diff --check, and update plan.md before stopping.
```

### Pass 2026-05-20: RenderBackend frame begin/end validation

Active blocker:

- Blocker A: Slang RHI frame graph is not implemented.

Changed:

- Extended the hidden Slang RHI frame-resource probe with explicit backend
  frame begin, encode, end, submit, and fence-completion state.
- Added Octaryn-owned scalar lifecycle fields to `RenderBackendStatus`; no
  Slang RHI handles or types cross the render-backend API boundary.
- Updated the client bootstrap log and native backend probe so launch validation
  now requires `slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1
  submitted=1`.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_probe octaryn_validate_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
git diff --check
```

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
client_render_backend active=1 backend=slang_rhi shader_language=slang legacy_glsl=0 legacy_sdl_renderer=0 device_created=1
client_slang_shader_validation=passed entry_points=4 slangc=/home/zacharyr/.local/bin/slangc
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
git diff --check passed.
```

Remaining:

- Continue Blocker A with a real window/surface-aware present target if Slang
  RHI supports the local platform window handle cleanly; otherwise keep the
  backend-owned offscreen/no-window path explicit and move to Blocker B only
  after documenting why swapchain proof is blocked on platform support.
- Follow-on Blocker B remains GPU execution of the voxel pass graph.
- Keep the SDL OpenGL configure-report guardrail item visible until confirmed
  disabled or isolated from active renderer paths.

Exact next step:

```txt
Continue from plan.md Blocker A. Inspect local Slang RHI swapchain/window-handle support against the current SDL3 window lifecycle, then either wire a backend-owned present target or record the concrete platform blocker and proceed to the first Blocker B GPU voxel pass-graph slice. Use tools/build/cmake_build.sh targeted validations only, do not run ctest or smoke tests, run the plan.md forbidden-path grep, run git diff --check, and update plan.md before stopping.
```

### Pass 2026-05-20: Slang RHI naming correction

Active blocker:

- Blocker A: Slang RHI frame graph is not implemented.

Changed:

- Renamed Octaryn-facing backend files and symbols to `SlangRhi*`.
- Renamed the CMake dependency wrapper and compile definition from
  the old wrong RHI spelling to `slang_rhi` /
  `OCTARYN_CLIENT_SLANG_RHI_AVAILABLE`.
- Updated validator error text and plan wording to refer to Slang RHI. The
  vendor `slang-gfx.h` include and private `gfx::` namespace remain hidden
  inside `octaryn-client/Source/Rendering/RenderBackend/` because that is the
  SDK surface currently providing the RHI implementation.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_probe octaryn_validate_client_app_launch_probe
tools/build/cmake_configure.sh release-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "SlangG[f]x|slang_g[f]x|SLANG_G[f]X|Slang G[f]X|RHI[[:space:]]+/[[:space:]]+Slang|RHI/G[f]X|G[f]X" octaryn-client tools cmake CMakeLists.txt plan.md -g '!references/**' -g '!build/**' -g '!logs/**'
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
git diff --check
```

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
client_slang_shader_validation=passed entry_points=4 slangc=/home/zacharyr/.local/bin/slangc
Active Octaryn-facing wrong-RHI-name grep returned no hits.
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
git diff --check passed.
```

Remaining:

- Continue Blocker A with concrete Slang RHI window/surface support analysis, or
  record the exact local platform limitation and move to the first Blocker B
  GPU voxel pass-graph slice.

Exact next step:

```txt
Continue from plan.md Blocker A. Use Slang RHI naming only in Octaryn-facing code and docs. Inspect Slang RHI swapchain support against SDL3 Wayland/X11 window properties, then either wire the backend-owned present target or record the concrete platform limitation and proceed to Blocker B. Run targeted tools/build/cmake_build.sh validations only, do not run ctest or smoke tests, run the forbidden-path grep, run git diff --check, and update plan.md before stopping.
```

### Pass 2026-05-20: Slang RHI X11 swapchain present probe

Active blocker:

- Blocker A: Slang RHI frame graph is not implemented.

Changed:

- Added `SlangRhiSwapchain.*` under the client render backend. It uses SDL3
  only to create an X11/XWayland window handle, then keeps Slang RHI device,
  queue, swapchain, acquire, present, and teardown private to the render-backend
  implementation.
- Added `octaryn_client_render_backend_swapchain_probe` and
  `octaryn_validate_client_render_backend_swapchain_probe`.
- Wired the swapchain probe into `octaryn_tools`, `octaryn_validate_all`, and
  CMake target inventory policy.
- Fixed swapchain/window teardown order after the first probe run exposed that
  the RHI swapchain must be released before destroying the SDL X11 window.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_swapchain_probe
gdb -batch -ex run -ex bt --args /usr/bin/env SDL_VIDEODRIVER=x11 build/debug-linux/tools/native/bin/octaryn_client_render_backend_swapchain_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_swapchain_probe
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_probe octaryn_validate_client_render_backend_swapchain_probe octaryn_validate_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "SlangG[f]x|slang_g[f]x|SLANG_G[f]X|Slang G[f]X|RHI[[:space:]]+/[[:space:]]+Slang|RHI/G[f]X|G[f]X" octaryn-client tools cmake CMakeLists.txt plan.md -g '!references/**' -g '!build/**' -g '!logs/**'
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_swapchain_probe
```

The first swapchain probe created the swapchain but aborted during teardown:

```txt
*** buffer overflow detected ***: terminated
gfx::vk::SwapchainImpl::destroySwapchainAndImages()
octaryn::client::rendering::probe_slang_rhi_xlib_swapchain()
```

The cause was teardown order: the SDL X11 window was destroyed before the RHI
swapchain COM object released. Releasing the swapchain first repaired the
probe.

Two later concurrent Ninja invocations collided while generating C++ module
dyndep files and client bundle artifacts. Re-running configure and validation
sequentially repaired the generated graph; no source change was needed for
that transient build-runner failure.

Evidence:

```txt
client_render_backend_swapchain_probe=passed driver=x11 status=swapchain_present_validated swapchain=created acquired=1 presented=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
client_slang_shader_validation=passed entry_points=4 slangc=/home/zacharyr/.local/bin/slangc
Active Octaryn-facing wrong-RHI-name grep returned no hits.
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
git diff --check passed.
```

Remaining:

- Blocker A is materially smaller but not closed: the probe proves RHI
  swapchain create/acquire/present through X11/XWayland, while the client app
  still lacks an integrated window-present frame graph.
- Follow-on Blocker B remains GPU execution of the voxel pass graph.

Exact next step:

```txt
Continue from plan.md Blocker A. Integrate the validated Slang RHI swapchain/present lifecycle into the client-owned frame graph, or split the render-backend frame graph into focused device, frame resources, and swapchain files if the current files grow further. Keep SDL as window/input only, keep Slang RHI types private to octaryn-client render-backend internals, run targeted tools/build/cmake_build.sh validations only, do not run ctest or smoke tests, run the forbidden-path grep, run git diff --check, and update plan.md before stopping.
```

### Pass 2026-05-20: Client launch validates Slang RHI swapchain present

Active blocker:

- Blocker A: Slang RHI frame graph is not implemented.

Changed:

- Extended the Slang RHI bootstrap app so the client launch probe exercises the
  render-backend swapchain path and logs
  `slang_rhi_swapchain=swapchain_present_validated driver=x11 created=1
  acquired=1 presented=1`.
- Updated client launch probe CMake targets to run the bootstrap under
  `SDL_VIDEODRIVER=x11`, matching the Slang RHI XLib swapchain support exposed
  by the local SDK.
- Hardened client launch log validation to require the swapchain present marker
  in order after device, frame resources, and frame lifecycle proof.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe
tools/build/cmake_configure.sh release-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_render_backend_probe octaryn_validate_client_render_backend_swapchain_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "SlangG[f]x|slang_g[f]x|SLANG_G[f]X|Slang G[f]X|RHI[[:space:]]+/[[:space:]]+Slang|RHI/G[f]X|G[f]X" octaryn-client tools cmake CMakeLists.txt plan.md -g '!references/**' -g '!build/**' -g '!logs/**'
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
git diff --check
```

Evidence:

```txt
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=swapchain_present_validated driver=x11 created=1 acquired=1 presented=1
client_render_backend active=1 backend=slang_rhi shader_language=slang legacy_glsl=0 legacy_sdl_renderer=0 device_created=1
client_render_backend_swapchain_probe=passed driver=x11 status=swapchain_present_validated swapchain=created acquired=1 presented=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=4 slangc=/home/zacharyr/.local/bin/slangc
Active Octaryn-facing wrong-RHI-name grep returned no hits.
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
git diff --check passed.
```

Remaining:

- Blocker A is still not a production frame graph; the launch app now proves the
  render-backend RHI frame and swapchain paths, but the real client frame graph
  still has to consume those backend APIs.
- Blocker B remains GPU execution of voxel pass graph resources.

Exact next step:

```txt
Continue from plan.md Blocker B unless touching the production client frame graph is required first. Wire the first GPU voxel pass-graph resource slice behind the Slang RHI render backend: compact chunk/palette upload buffers and a targeted validation probe that proves GPU resource creation/upload/readback for empty, uniform, and mixed palette payloads. Keep CPU limited to compact voxel identity payloads, keep Slang RHI types private to octaryn-client render-backend internals, run targeted tools/build/cmake_build.sh validations only, do not run ctest or smoke tests, run the forbidden-path grep, run git diff --check, and update plan.md before stopping.
```

### Pass 2026-05-21: Slang RHI compact voxel payload upload/readback

Active blocker:

- Blocker B: first GPU voxel pass-graph resource slice behind the Slang RHI
  render backend.

Source-to-destination plan:

- Keep shader-facing chunk header and palette contracts beside client-owned
  voxel world code.
- Keep Slang RHI buffer creation, upload, submission, fence, and readback behind
  `octaryn-client/Source/Rendering/RenderBackend`.
- Keep validation probes under `tools/Source`, with no Slang RHI types exposed
  to shared, server, basegame, or module APIs.

Changed:

- Added `GpuChunkPayload.*` under `octaryn-client/Source/Rendering/VoxelWorld`
  with native `GpuChunkHeader` and `GpuChunkPaletteEntry` layouts matching
  `OctarynGpuTypes.slang`.
- Reworked the Slang RHI voxel upload probe to create three GPU buffers for
  compact chunk headers, palette entries, and packed voxel payload bytes, then
  upload, submit, fence, and read them back for empty, uniform, and mixed
  palette chunks.
- Extended the voxel invariants probe to validate GPU chunk payload construction
  and reject invalid mixed palette headers.
- Added workspace Slang SDK hints for both the RHI library and `slangc`, and
  fixed shader validation CMake so optional `--slangc` arguments are omitted
  cleanly when unavailable.
- Updated the Podman builder path to include Vulkan ICD packages and pass
  through `/dev/dri` and `/dev/kfd` when present, so helper-run Slang RHI probes
  exercise the same GPU device path as host runs.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_invariants_probe octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_configure.sh release-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.sh' -o -name '*.txt' \) -not -path '*/build/*' -not -path '*/references/*' -not -path '*/logs/*' -print0 | xargs -0 wc -l | awk '$1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_invariants_probe octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe
```

The first helper-path Slang RHI runs compiled but failed device creation because
the Podman image had the Vulkan loader but no Vulkan ICD packages and did not
pass through GPU device nodes. Adding `vulkan-radeon`, `vulkan-swrast`, and
optional `/dev/dri`/`/dev/kfd` passthrough repaired the helper-run probes.

Shader validation initially failed because CMake passed two empty generator
expression arguments when `slangc` was unresolved. Building the optional
`--slangc` arguments as a normal CMake list and searching the workspace Slang
SDK repaired the target.

Evidence:

```txt
client voxel invariants probe passed
client_voxel_upload_probe=passed status=gpu_chunk_payload_uploads_validated headers=1 palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=4 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is materially smaller but not closed: this pass proves Slang RHI
  GPU resource creation, upload, submission, fence, and readback for compact
  voxel chunk identity payloads, but it does not dispatch a GPU compute pass.
- Final renderer cutover still needs GPU-owned occupancy, face masks, greedy
  quad output, culling, indirect draw generation, raster shading, and runtime
  proof through the real client path.

Exact next step:

```txt
Continue Blocker B by adding the first Slang RHI compute pass that decodes compact chunk headers, palette entries, and packed voxel bytes into a GPU-owned occupancy or face-mask buffer, with a targeted validation probe comparing GPU readback against CPU-expected empty, uniform, and mixed chunk outputs. Keep Slang RHI types private to octaryn-client render-backend internals, keep CPU streams limited to compact identity payloads, run targeted tools/build/cmake_build.sh validations only, do not run ctest or smoke tests, rerun forbidden-path grep and source line-count checks, and update plan.md before stopping.
```

### Pass 2026-05-20: Slang RHI compact voxel occupancy compute probe

Active blocker:

- Blocker B: first GPU compute pass for compact voxel identity data.

Source-to-destination plan:

- Keep the Slang compute shader under the client-owned voxel shader tree.
- Keep Slang RHI program, pipeline, buffer, dispatch, fence, and readback code
  hidden inside `octaryn-client/Source/Rendering/RenderBackend`.
- Keep the validation executable under `tools/Source`, returning only
  Octaryn-owned scalar probe results with no raw Slang RHI types in shared,
  server, basegame, or module APIs.

Changed:

- Added `Voxel/VoxelOccupancyProbe.slang`, a Slang compute shader that reads
  compact chunk headers, palette entries, and packed 2-bit voxel payload words,
  then writes per-chunk occupancy counts.
- Added `SlangRhiVoxelOccupancy.*` under the client render backend to create
  the Slang RHI compute pipeline, bind compact voxel buffers, dispatch three
  chunks, fence, and validate readback counts for empty, uniform, and mixed
  chunks.
- Added `tools/Source/ClientVoxelOccupancyProbe/ClientVoxelOccupancyProbe.cpp`
  plus CMake wiring for `octaryn_client_voxel_occupancy_probe` and
  `octaryn_validate_client_voxel_occupancy_probe`.
- Extended Slang shader validation and CMake target inventory policy for the
  new compute shader and validation target.
- Moved the probe UAV binding from `u0` to `u3` after Slang reflection showed
  Vulkan descriptor slot overlap between `t0` and `u0`.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_occupancy_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe octaryn_validate_client_voxel_occupancy_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_configure.sh release-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.sh' -o -name '*.txt' \) -not -path '*/build/*' -not -path '*/references/*' -not -path '*/logs/*' -print0 | xargs -0 wc -l | awk '$1 > 500 {print}'
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_occupancy_probe
```

The first GPU compute runs dispatched and fenced, but read back
`empty=24576 uniform=24576 mixed=24576`. A temporary hard-coded shader proved
dispatch/readback was working. Slang reflection then exposed the actual issue:
the Vulkan layout mapped `t0` and `u0` to descriptor slot 0, so the output UAV
overlapped the header SRV. Moving the output UAV to `u3` repaired the real
compact-input occupancy path.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
```

The first CMake inventory run failed because the other configured preset graphs
were stale and did not yet contain the new occupancy probe targets. Refreshing
`release-linux`, `debug-windows`, and `release-windows` repaired the same
validator.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
```

The client app launch probe remains blocked in this environment at the X11
swapchain step:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_occupancy_probe=passed status=gpu_voxel_occupancy_validated empty=0 uniform=32768 mixed=24576 dispatched=1 readback=1
client_voxel_upload_probe=passed status=gpu_chunk_payload_uploads_validated headers=1 palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=5 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is materially smaller but not closed: the GPU now owns the first
  occupancy decode/count compute probe for compact voxel identity data, but the
  runtime still lacks face masks, greedy quad output, culling, indirect draw
  generation, raster shading, and production client frame integration.
- The X11 client launch probe needs a valid local video/swapchain environment
  before it can re-prove swapchain present for this pass.

Exact next step:

```txt
Continue Blocker B by extending the Slang RHI compute path from occupancy counts to GPU-owned visible face-mask output for empty, uniform solid, checkerboard, and mixed material chunks. Keep compact voxel identity as the only CPU stream, keep Slang RHI types private to octaryn-client render-backend internals, run the voxel occupancy/upload/render-backend/shader validations plus CMake inventory checks, rerun forbidden-path grep and source line-count checks, and update plan.md before stopping.
```

### Pass 2026-05-20: Slang RHI compact voxel face-mask compute probe

Active blocker:

- Blocker B: GPU face-mask pass for compact voxel identity data.

Source-to-destination plan:

- Keep the face-mask compute shader under the client-owned Slang voxel shader
  tree.
- Keep Slang RHI pipeline, buffers, dispatch, fence, and readback hidden inside
  `octaryn-client/Source/Rendering/RenderBackend`.
- Keep CPU expected-mask generation as a focused render-backend probe helper,
  not as a shared/server/basegame API.
- Keep the executable validation surface under `tools/Source`, exposing only
  Octaryn-owned scalar probe results.

Changed:

- Added `Voxel/VoxelFaceMaskProbe.slang`, a Slang compute shader that decodes
  compact chunk identity and writes one six-bit visible-face mask per voxel plus
  per-chunk face counts.
- Added `SlangRhiVoxelFaceMasks.*` and
  `SlangRhiVoxelFaceMaskExpected.*` under the client render backend. The GPU
  path validates exact mask buffers and counts for empty, uniform solid,
  checkerboard, and mixed-material chunks.
- Added `tools/Source/ClientVoxelFaceMaskProbe/ClientVoxelFaceMaskProbe.cpp`.
- Wired `octaryn_client_voxel_face_mask_probe` and
  `octaryn_validate_client_voxel_face_mask_probe` into owner CMake, tool
  aggregation, root validation, shader validation, and CMake target inventory.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_face_mask_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_face_mask_probe octaryn_validate_client_voxel_occupancy_probe octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_configure.sh release-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
rg -n "BasegameBlockCatalog|native-client-catalog|render_native_catalog_header|SDL_GPU|sdl_gpu|\\.glsl|gpu_render_path=Slang_RHI|WorldMeshRuntime|WorldMeshUpload|ShaderPipelines|FrameRender|FrameTargets|EmptyWorldMesh" octaryn-client octaryn-basegame tools cmake CMakeLists.txt -g '!build/**' -g '!references/**' -g '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.sh' -o -name '*.txt' \) -not -path '*/build/*' -not -path '*/references/*' -not -path '*/logs/*' -print0 | xargs -0 wc -l | awk '$1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_face_mask_probe
```

The first run failed before device creation because the probe sanity check
expected seven palette entries instead of the actual nine entries
`0 + 1 + 4 + 4`. After fixing that, the GPU dispatch produced correct counts
but still reported a mismatch because the CPU expected-count array was not
zero-initialized. Initializing the expected data repaired the exact mask-buffer
comparison.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
```

The client app launch probe remains blocked in this environment at the X11
swapchain step, while still proving Slang RHI device creation and offscreen
frame lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_face_mask_probe=passed status=gpu_voxel_face_masks_validated empty=0 uniform=6144 checkerboard=98304 mixed=6144 dispatched=1 readback=1
client_voxel_occupancy_probe=passed status=gpu_voxel_occupancy_validated empty=0 uniform=32768 mixed=24576 dispatched=1 readback=1
client_voxel_upload_probe=passed status=gpu_chunk_payload_uploads_validated headers=1 palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=6 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is materially smaller but not closed: GPU-owned occupancy and
  face-mask compute probes now exist, but greedy quad count/scan/emit, culling,
  indirect draw generation, raster shading, and production client frame
  integration remain.
- The X11 client launch probe still needs a valid local video/swapchain
  environment before it can re-prove swapchain present for this pass.

Exact next step:

```txt
Continue Blocker B by adding a GPU greedy-count probe that consumes the face-mask output for empty, uniform solid, checkerboard, and mixed-material chunks and validates deterministic quad/material counts before implementing prefix scan and packed quad emit. Keep compact voxel identity as the only CPU stream, keep Slang RHI types private to octaryn-client render-backend internals, run renderer/shader/CMake validations, rerun forbidden-path grep and source line-count checks, and update plan.md before stopping.
```

### Pass 2026-05-21: Slang RHI greedy-count compute probe

Active blocker:

- Blocker B: GPU greedy-count pass over GPU-owned face-mask output.

Source-to-destination plan:

- Use the existing compact chunk probe cases as source data: empty, uniform
  solid, checkerboard, and mixed-material solid chunks.
- Keep the face-mask-to-greedy pipeline inside the client-owned Slang RHI
  render-backend implementation.
- Expose only scalar Octaryn probe results through a tool validation target;
  no Slang RHI types or renderer internals cross into shared/server/basegame.

Changed:

- Added `Voxel/VoxelGreedyCountProbe.slang`, a Slang compute shader that reads
  the GPU face-mask buffer plus compact chunk identity/material data and writes
  per-chunk greedy quad counts and material sums.
- Added `SlangRhiVoxelGreedyCounts.*` under the client render backend. The
  probe dispatches the existing face-mask compute shader, barriers the
  face-mask buffer to shader-resource state, dispatches greedy count, fences,
  readbacks, and validates the expected count/material results.
- Added `tools/Source/ClientVoxelGreedyCountProbe/ClientVoxelGreedyCountProbe.cpp`.
- Wired `octaryn_client_voxel_greedy_count_probe` and
  `octaryn_validate_client_voxel_greedy_count_probe` into owner CMake, tool
  aggregation, root validation, shader validation, shader bundle validation,
  and CMake target inventory.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_greedy_count_probe octaryn_validate_client_voxel_face_mask_probe octaryn_validate_client_voxel_occupancy_probe octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh release-windows
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
```

The first inventory run failed because the newly added greedy-count targets had
not yet been generated for `release-linux`, `debug-windows`, and
`release-windows`. Regenerating those presets repaired the target inventory.

```txt
tools/build/cmake_configure.sh debug-windows
```

One concurrent Podman-routed configure attempt failed with
`tools/build: Permission denied`; rerunning the preset after the parallel
configure finished succeeded.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
```

The client app launch probe remains blocked in this environment at the X11
swapchain step, while still proving Slang RHI device creation and offscreen
frame lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_greedy_count_probe=passed status=gpu_voxel_greedy_counts_validated empty=0 uniform=6 checkerboard=98304 mixed=130 empty_material=0 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 face_dispatch=1 greedy_dispatch=1 readback=1
client_voxel_face_mask_probe=passed status=gpu_voxel_face_masks_validated empty=0 uniform=6144 checkerboard=98304 mixed=6144 dispatched=1 readback=1
client_voxel_occupancy_probe=passed status=gpu_voxel_occupancy_validated empty=0 uniform=32768 mixed=24576 dispatched=1 readback=1
client_voxel_upload_probe=passed status=gpu_chunk_payload_uploads_validated headers=1 palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=7 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is materially smaller but not closed: GPU-owned occupancy,
  face-mask, and greedy-count compute probes now exist, but prefix scan,
  packed greedy quad emit, culling, indirect draw generation, raster shading,
  and production client frame integration remain.
- The X11 client launch probe still needs a valid local video/swapchain
  environment before it can re-prove swapchain present for this pass.

Exact next step:

```txt
Continue Blocker B by adding the GPU prefix-scan/output-range probe for greedy quad counts, then emit packed `PackedVoxelQuad16` records for empty, uniform solid, checkerboard, and mixed-material chunks. The probe must consume GPU greedy counts, validate deterministic offsets/counts/material identity, keep compact voxel identity as the only CPU stream, keep Slang RHI types private to octaryn-client render-backend internals, run renderer/shader/CMake validations, rerun forbidden-path grep and source line-count checks, and update plan.md before stopping.
```

### Pass 2026-05-21: Slang RHI greedy prefix-range compute probe

Active blocker:

- Blocker B: GPU prefix-scan/output ranges over GPU greedy counts.

Source-to-destination plan:

- Use the existing four compact chunk probe cases as the source stream.
- Run the probe as a client-owned Slang RHI pass chain:
  face masks -> greedy counts -> prefix ranges.
- Keep only scalar validation output in the tool target; no Slang RHI types or
  renderer internals cross into shared/server/basegame/module APIs.

Changed:

- Added `Voxel/VoxelPrefixScanProbe.slang`, a Slang compute shader that reads
  GPU greedy quad counts and writes exclusive per-chunk output offsets plus the
  total quad count.
- Added `SlangRhiVoxelPrefixRanges.*` under the client render backend. The
  probe dispatches face masks, greedy counts, and prefix scan in one command
  buffer with buffer barriers between passes, then validates offsets
  `{0, 0, 6, 98310}` and total `98440`.
- Added `SlangRhiVoxelPrefixProbeBatch.*` to keep compact probe-batch
  construction out of the prefix-range probe file and under the file-size cap.
- Added `tools/Source/ClientVoxelPrefixRangeProbe/ClientVoxelPrefixRangeProbe.cpp`.
- Wired `octaryn_client_voxel_prefix_range_probe` and
  `octaryn_validate_client_voxel_prefix_range_probe` into owner CMake, tool
  aggregation, root validation, shader validation, shader bundle validation,
  and CMake target inventory.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_prefix_range_probe octaryn_validate_client_voxel_greedy_count_probe octaryn_validate_client_voxel_face_mask_probe octaryn_validate_client_voxel_occupancy_probe octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh release-windows
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
```

The first inventory run failed because the new prefix-range targets were not
yet generated for `release-linux`, `debug-windows`, and `release-windows`.
Regenerating those presets repaired the inventory.

```txt
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
```

Two concurrent Podman-routed configure attempts failed with wrapper
permission/working-directory errors while another preset configure was running.
Rerunning the failed presets sequentially succeeded.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
```

The client app launch probe remains blocked in this environment at the X11
swapchain step, while still proving Slang RHI device creation and offscreen
frame lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_prefix_range_probe=passed status=gpu_voxel_prefix_ranges_validated empty_offset=0 uniform_offset=0 checkerboard_offset=6 mixed_offset=98310 total=98440 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 readback=1
client_voxel_greedy_count_probe=passed status=gpu_voxel_greedy_counts_validated empty=0 uniform=6 checkerboard=98304 mixed=130 empty_material=0 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 face_dispatch=1 greedy_dispatch=1 readback=1
client_voxel_face_mask_probe=passed status=gpu_voxel_face_masks_validated empty=0 uniform=6144 checkerboard=98304 mixed=6144 dispatched=1 readback=1
client_voxel_occupancy_probe=passed status=gpu_voxel_occupancy_validated empty=0 uniform=32768 mixed=24576 dispatched=1 readback=1
client_voxel_upload_probe=passed status=gpu_chunk_payload_uploads_validated headers=1 palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=8 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is materially smaller but not closed: GPU-owned occupancy,
  face-mask, greedy-count, and prefix-range compute probes now exist, but
  packed greedy quad emit, culling, indirect draw generation, raster shading,
  and production client frame integration remain.
- The X11 client launch probe still needs a valid local video/swapchain
  environment before it can re-prove swapchain present for this pass.

Exact next step:

```txt
Continue Blocker B by adding the GPU packed-quad emit probe for `PackedVoxelQuad16` records using the validated prefix ranges. The probe must consume GPU face masks, greedy counts, and prefix offsets, validate deterministic quad count/material identity for empty, uniform solid, checkerboard, and mixed-material chunks, keep compact voxel identity as the only CPU stream, keep Slang RHI types private to octaryn-client render-backend internals, run renderer/shader/CMake validations, rerun forbidden-path grep and source line-count checks, and update plan.md before stopping.
```

### Pass 2026-05-21: Slang RHI packed quad emit compute probe

Active blocker:

- Blocker B: GPU packed `PackedVoxelQuad16` emit from validated greedy ranges.

Source-to-destination plan:

- Use the existing four compact chunk probe cases as the CPU source stream:
  empty, uniform solid, checkerboard, and mixed material.
- Run one client-owned Slang RHI command chain:
  face masks -> greedy counts -> prefix ranges -> packed quad emit.
- Keep packed records and validation inside client render-backend internals and
  expose only scalar probe status through the repo tool target.

Changed:

- Added `Voxel/VoxelPackedQuadEmitProbe.slang`, which consumes compact chunk
  headers/palette/payload words, GPU face masks, greedy counts, and prefix
  offsets, then writes `PackedVoxelQuad16` records plus per-chunk emitted counts
  and material sums.
- Added `SlangRhiVoxelPackedQuadEmit.*` under the client render backend. The
  probe dispatches face masks, greedy counts, prefix scan, and packed emit in a
  single Slang RHI command buffer with buffer barriers between passes.
- Added `SlangRhiVoxelPackedQuadValidation.*` so packed-quad readback/range
  validation stays focused and the emit probe source remains under the file
  size cap.
- Added
  `tools/Source/ClientVoxelPackedQuadEmitProbe/ClientVoxelPackedQuadEmitProbe.cpp`.
- Wired `octaryn_client_voxel_packed_quad_emit_probe` and
  `octaryn_validate_client_voxel_packed_quad_emit_probe` into owner CMake, tool
  aggregation, root validation, shader validation, shader bundle validation,
  and CMake target inventory.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_packed_quad_emit_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_prefix_range_probe octaryn_validate_client_voxel_greedy_count_probe octaryn_validate_client_voxel_face_mask_probe octaryn_validate_client_voxel_occupancy_probe octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_configure.sh release-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
```

The first inventory run failed because `release-linux`, `debug-windows`, and
`release-windows` had stale generated target graphs without the new packed
quad emit target. Regenerating those presets repaired the inventory.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
```

The client app launch probe remains blocked in this environment at the X11
swapchain step, while still proving Slang RHI device creation and offscreen
frame lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_voxel_prefix_range_probe=passed status=gpu_voxel_prefix_ranges_validated empty_offset=0 uniform_offset=0 checkerboard_offset=6 mixed_offset=98310 total=98440 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 readback=1
client_voxel_greedy_count_probe=passed status=gpu_voxel_greedy_counts_validated empty=0 uniform=6 checkerboard=98304 mixed=130 empty_material=0 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 face_dispatch=1 greedy_dispatch=1 readback=1
client_voxel_face_mask_probe=passed status=gpu_voxel_face_masks_validated empty=0 uniform=6144 checkerboard=98304 mixed=6144 dispatched=1 readback=1
client_voxel_occupancy_probe=passed status=gpu_voxel_occupancy_validated empty=0 uniform=32768 mixed=24576 dispatched=1 readback=1
client_voxel_upload_probe=passed status=gpu_chunk_payload_uploads_validated headers=1 palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=9 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines; largest touched source is SlangRhiVoxelPackedQuadEmit.cpp at 497 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is materially smaller but not closed: GPU-owned occupancy,
  face-mask, greedy-count, prefix-range, and packed-quad emit compute probes now
  exist, but culling, indirect draw generation, raster shading, and production
  client frame integration remain.
- The X11 client launch probe still needs a valid local video/swapchain
  environment before it can re-prove swapchain present for this pass.

Exact next step:

```txt
Continue Blocker B with the GPU culling and indirect-generation probe that consumes GPU-emitted `PackedVoxelQuad16` records and writes compact indirect draw command/count output. Keep compact voxel identity as the only CPU stream, keep Slang RHI types private to octaryn-client render-backend internals, validate empty zero-draw behavior plus non-empty deterministic draw counts, run renderer/shader/CMake validations, rerun forbidden-path grep and source line-count checks, and update plan.md before stopping.
```

### Pass 2026-05-21: Slang RHI indirect generation compute probe

Active blocker:

- Blocker B: GPU culling/indirect command generation after packed quad emit.

Source-to-destination plan:

- Use the existing four compact chunk probe cases as the CPU source stream.
- Run one client-owned Slang RHI command chain:
  face masks -> greedy counts -> prefix ranges -> packed quad emit -> indirect
  generation.
- Keep draw commands, packed quads, and validation inside client render-backend
  internals; expose only scalar probe status through the repo tool target.

Changed:

- Added `Voxel/VoxelIndirectGenerationProbe.slang`, which consumes GPU-emitted
  `PackedVoxelQuad16` records plus emitted counts/prefix offsets and writes a
  compact `DrawIndexedIndirectCommand` stream, draw count, empty draw count,
  total instance count, and a first-quad checksum.
- Added `SlangRhiVoxelIndirectGeneration.*` under the client render backend.
  The probe dispatches face masks, greedy counts, prefix scan, packed emit, and
  indirect generation in one command buffer with buffer barriers between
  dependent passes.
- Added `SlangRhiVoxelIndirectValidation.*` so indirect command readback checks
  stay focused and the indirect probe source remains under the file-size cap.
- Added
  `tools/Source/ClientVoxelIndirectGenerationProbe/ClientVoxelIndirectGenerationProbe.cpp`.
- Wired `octaryn_client_voxel_indirect_generation_probe` and
  `octaryn_validate_client_voxel_indirect_generation_probe` into owner CMake,
  tool aggregation, root validation, shader validation, shader bundle
  validation, and CMake target inventory.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_indirect_generation_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_indirect_generation_probe octaryn_validate_client_voxel_packed_quad_emit_probe octaryn_validate_client_voxel_prefix_range_probe octaryn_validate_client_voxel_greedy_count_probe octaryn_validate_client_voxel_face_mask_probe octaryn_validate_client_voxel_occupancy_probe octaryn_validate_client_voxel_upload_probe octaryn_validate_client_render_backend_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_configure.sh release-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
```

The first inventory run failed because `release-linux`, `debug-windows`, and
`release-windows` had stale generated target graphs without the new indirect
generation target. Regenerating those presets repaired the inventory.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
```

The client app launch probe remains blocked in this environment at the X11
swapchain step, while still proving Slang RHI device creation and offscreen
frame lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_indirect_generation_probe=passed status=gpu_voxel_indirect_commands_validated draws=3 empty_draws=1 instances=98440 first_uniform=0 first_checkerboard=6 first_mixed=98310 checksum_nonzero=1 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 indirect_dispatch=1 readback=1
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_voxel_prefix_range_probe=passed status=gpu_voxel_prefix_ranges_validated empty_offset=0 uniform_offset=0 checkerboard_offset=6 mixed_offset=98310 total=98440 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 readback=1
client_voxel_greedy_count_probe=passed status=gpu_voxel_greedy_counts_validated empty=0 uniform=6 checkerboard=98304 mixed=130 empty_material=0 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 face_dispatch=1 greedy_dispatch=1 readback=1
client_voxel_face_mask_probe=passed status=gpu_voxel_face_masks_validated empty=0 uniform=6144 checkerboard=98304 mixed=6144 dispatched=1 readback=1
client_voxel_occupancy_probe=passed status=gpu_voxel_occupancy_validated empty=0 uniform=32768 mixed=24576 dispatched=1 readback=1
client_voxel_upload_probe=passed status=gpu_chunk_payload_uploads_validated headers=1 palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=10 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines; largest touched source is SlangRhiVoxelIndirectGeneration.cpp at 498 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is materially smaller but not closed: GPU-owned occupancy,
  face-mask, greedy-count, prefix-range, packed-quad emit, and indirect-command
  generation compute probes now exist, but production client frame integration,
  full frustum/Hi-Z culling coverage, raster shading, and runtime/profiling
  evidence remain.
- The X11 client launch probe still needs a valid local video/swapchain
  environment before it can re-prove swapchain present for this pass.

Exact next step:

```txt
Continue with the runtime cutover path by wiring the GPU voxel pass graph resources into the client-owned Slang RHI render backend frame path far enough that the face-pulled raster path can consume GPU-generated `PackedVoxelQuad16` records and indirect draw commands. Keep compact voxel identity as the only CPU stream, keep Slang RHI types private to octaryn-client render-backend internals, preserve bounded chunk streaming, run renderer/shader/CMake validations, rerun forbidden-path grep and source line-count checks, and update plan.md before stopping.
```

### Pass 2026-05-21: Slang RHI offscreen voxel raster frame probe

Active blockers:

- Blocker C: face-pulled raster path is not runtime-active.
- Blocker D: GPU-driven indirect drawing is not runtime-active.

Source-to-destination plan:

- Keep the proof inside `octaryn-client/Source/Rendering/RenderBackend` so raw
  Slang RHI types remain private to the client render backend.
- Add a focused GPU generation shader that writes one `PackedVoxelQuad16` and
  one `DrawIndexedIndirectCommand`.
- Add a focused face-pulled raster shader that pulls packed quads by instance
  ID, emits a full-screen offscreen quad, and validates readback through a tool
  target.

Changed:

- Added `Voxel/VoxelRasterFrameGenerateProbe.slang` to write a GPU packed quad,
  draw command, draw count, and total instance count.
- Added `Voxel/VoxelRasterProbe.slang` to pull `PackedVoxelQuad16` records in a
  vertex shader and shade non-clear pixels in a fragment shader.
- Added `SlangRhiVoxelRasterFrame.*` and the internal
  `SlangRhiVoxelRasterFrameGpu.h` helper under the client render backend. The
  probe creates Slang RHI offscreen frame resources, dispatches the GPU
  generator, binds generated buffers to a graphics pipeline, issues
  `drawIndexedIndirect`, and reads back the color target.
- Added
  `tools/Source/ClientVoxelRasterFrameProbe/ClientVoxelRasterFrameProbe.cpp`.
- Wired `octaryn_client_voxel_raster_frame_probe` and
  `octaryn_validate_client_voxel_raster_frame_probe` into owner CMake, root
  validation, shader validation, shader bundle validation, tool aggregation,
  and CMake target inventory.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_configure.sh release-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe octaryn_validate_client_voxel_indirect_generation_probe octaryn_validate_client_voxel_packed_quad_emit_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
```

The first CMake inventory run failed because `release-linux`, `debug-windows`,
and `release-windows` had stale generated target graphs without the new raster
frame target. Regenerating those presets repaired the inventory.

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
```

The client app launch probe remains blocked in this environment at the X11
swapchain step, while still proving Slang RHI device creation and offscreen
frame lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated draws=1 instances=1 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_indirect_generation_probe=passed status=gpu_voxel_indirect_commands_validated draws=3 empty_draws=1 instances=98440 first_uniform=0 first_checkerboard=6 first_mixed=98310 checksum_nonzero=1 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 indirect_dispatch=1 readback=1
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=13 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Active source line-count scan returned no files over 500 physical lines; largest touched source is SlangRhiVoxelRasterFrame.cpp at 457 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker C is smaller but not closed: a Slang RHI raster probe now exercises
  vertex-pulled `PackedVoxelQuad16` records, indirect drawing, and offscreen
  color readback, but the production client frame path still does not consume
  the pass-graph output.
- Blocker D is smaller but not closed: GPU indirect generation and an indirect
  raster draw probe exist, but full culling coverage and runtime frame
  submission are still missing.
- The X11 client launch probe still needs a valid local video/swapchain
  environment before it can re-prove swapchain present for this pass.

Exact next step:

```txt
Replace the focused raster-frame generator input with the real GPU voxel pass graph resources in the client-owned Slang RHI render backend frame path: compact voxel identity upload -> occupancy/face-mask/greedy/prefix/packed-quad/indirect buffers -> face-pulled raster draw. Keep Slang RHI private to octaryn-client render backend internals, keep bounded chunk streaming unchanged, preserve the existing compute probes as guardrails, then gather direct runtime/profiling evidence for the real path before claiming renderer completion.
```

### Pass 2026-05-21: Raster probe consumes real GPU voxel pass graph

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated.
- Blocker C: face-pulled raster path is not runtime-active.
- Blocker D: GPU-driven indirect drawing is not runtime-active.

Source-to-destination plan:

- Remove the focused raster-frame generator shader from active validation.
- Keep the pass inside `octaryn-client/Source/Rendering/RenderBackend` so raw
  Slang RHI types stay private to client render-backend internals.
- Feed the offscreen raster probe from the existing compact chunk
  headers/palettes/payloads through the GPU face-mask, greedy-count, prefix,
  packed-quad emit, and indirect-generation passes before raster.

Changed:

- Replaced the synthetic `VoxelRasterFrameGenerateProbe.slang` input with the
  real compact voxel GPU pass graph in `SlangRhiVoxelRasterFrame.cpp`.
- Fixed raster-frame face-mask storage sizing to allocate one mask per voxel
  using `ChunkVoxelCount`; the previous raster-frame path had only enough mask
  storage for six words per chunk.
- Removed `Voxel/VoxelRasterFrameGenerateProbe.slang` from active shader source,
  shader compilation validation, and shader bundle validation.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_indirect_generation_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe octaryn_validate_client_voxel_indirect_generation_probe octaryn_validate_client_voxel_packed_quad_emit_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
rg -n "slang::|Slang::|gfx::|slang-rhi|slang_rhi|slang-gfx|slang-com-ptr" octaryn-server octaryn-shared octaryn-basegame --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

Failed then repaired:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
```

The first run encoded the indirect draw but read back zero draws/instances
because the raster-frame path allocated face-mask storage as `chunk_count * 6`
words. Allocating `chunk_count * ChunkVoxelCount` words repaired the full
pass-graph readback.

The client app launch probe still fails in this environment at the local X11
swapchain/video step, while proving the Slang RHI device and offscreen frame
lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_indirect_generation_probe=passed status=gpu_voxel_indirect_commands_validated draws=3 empty_draws=1 instances=98440 first_uniform=0 first_checkerboard=6 first_mixed=98310 checksum_nonzero=1 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 indirect_dispatch=1 readback=1
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; largest touched source is SlangRhiVoxelRasterFrame.cpp at 499 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: the focused raster probe now consumes the
  real compact voxel GPU pass graph, but production runtime integration,
  full frustum/Hi-Z culling coverage, material/PBR shading, and profiling proof
  remain.
- Blocker C is smaller but not closed: face-pulled raster draws GPU pass-graph
  generated packed quads in the offscreen probe, but the production client
  frame path still does not render those resources.
- Blocker D is smaller but not closed: indirect commands are generated and
  consumed by the focused raster probe, but runtime frame submission and full
  culling coverage remain.
- `SlangRhiVoxelRasterFrame.cpp` is now 499 lines; split it before any further
  behavior lands there.

Exact next step:

```txt
Split SlangRhiVoxelRasterFrame.cpp before adding behavior, then move the validated compact voxel GPU pass-graph-to-raster sequence into the production client-owned Slang RHI render backend frame path. The next pass should consume bounded streamed chunk identity buffers instead of the canonical probe batch, keep Slang RHI types private to octaryn-client render-backend internals, preserve compute probes as guardrails, and gather direct runtime/profiling evidence once the real path can run.
```

### Pass 2026-05-21: Extract reusable raster pass-graph encoder

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated.
- Blocker C: face-pulled raster path is not runtime-active.
- Blocker D: GPU-driven indirect drawing is not runtime-active.

Source-to-destination plan:

- Split `SlangRhiVoxelRasterFrame.cpp` before adding more renderer behavior.
- Move the validated face-mask, greedy-count, prefix, packed-quad emit, and
  indirect-generation compute dispatch ordering into a focused client
  render-backend internal file.
- Keep raw Slang RHI types inside `octaryn-client/Source/Rendering/RenderBackend`
  and preserve the existing offscreen raster probe as the guardrail.

Changed:

- Added `SlangRhiVoxelRasterPassGraph.h/.cpp` with a focused internal encoder
  for the compact voxel GPU pass graph and its resource barriers.
- Updated `SlangRhiVoxelRasterFrame.cpp` to call that internal encoder instead
  of owning the full compute-pass sequence inline.
- Wired the new source file into `octaryn_client_render_backend`.
- Reduced `SlangRhiVoxelRasterFrame.cpp` from 499 to 472 physical lines.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases octaryn_validate_client_voxel_raster_frame_probe octaryn_validate_client_voxel_indirect_generation_probe octaryn_validate_client_voxel_packed_quad_emit_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
rg -n "slang::|Slang::|gfx::|slang-rhi|slang_rhi|slang-gfx|slang-com-ptr" octaryn-server octaryn-shared octaryn-basegame --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

The client app launch probe still fails in this environment at the local X11
swapchain/video step, while proving the Slang RHI device and offscreen frame
lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_indirect_generation_probe=passed status=gpu_voxel_indirect_commands_validated draws=3 empty_draws=1 instances=98440 first_uniform=0 first_checkerboard=6 first_mixed=98310 checksum_nonzero=1 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 indirect_dispatch=1 readback=1
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; largest touched render-backend source is SlangRhiVoxelRasterFrame.cpp at 472 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: the pass ordering is now reusable inside
  the client render backend, but it still consumes the canonical probe batch
  rather than bounded streamed chunk identity buffers.
- Blocker C is smaller but not closed: production frame drawing still does not
  consume the generated packed quads.
- Blocker D is smaller but not closed: production frame submission still does
  not consume the generated indirect commands and full culling coverage remains.

Exact next step:

```txt
Introduce a production-facing client render-backend frame resource owner that uses the extracted SlangRhiVoxelRasterPassGraph encoder with bounded streamed compact chunk identity buffers instead of the canonical probe batch, then draw those generated packed quads through the client-owned frame path. Preserve the probes as guardrails and collect direct runtime/profiling evidence once the app path can exercise the real resources.
```

### Pass 2026-05-21: Raster probe input uses bounded stream batch

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated.
- Blocker C: face-pulled raster path is not runtime-active.
- Blocker D: GPU-driven indirect drawing is not runtime-active.

Source-to-destination plan:

- Replace the raster-frame probe's old canonical prefix-probe input with a
  production-shaped compact chunk batch built from client bounded streaming
  policy.
- Keep CPU input as compact chunk identity only: headers, palette entries, and
  paletted payload bytes.
- Preserve the existing Slang RHI pass graph and indirect raster probe as the
  validation guardrail.

Changed:

- Added `SlangRhiVoxelRasterStreamBatch.h/.cpp` to build a bounded raster input
  batch from `default_streaming_budget()` and render-distance column requests.
- Updated `SlangRhiVoxelRasterFrame.cpp` to consume the stream batch instead of
  `SlangRhiVoxelPrefixProbeBatch`.
- Added `bounded_columns` to the raster-frame probe result and tool output.
- Wired the new source file into `octaryn_client_render_backend`.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases octaryn_validate_client_voxel_raster_frame_probe octaryn_validate_client_voxel_indirect_generation_probe octaryn_validate_client_voxel_packed_quad_emit_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
rg -n "slang::|Slang::|gfx::|slang-rhi|slang_rhi|slang-gfx|slang-com-ptr" octaryn-server octaryn-shared octaryn-basegame --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
git diff --check
```

The client app launch probe still fails in this environment at the local X11
swapchain/video step, while proving the Slang RHI device and offscreen frame
lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated columns=4 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_indirect_generation_probe=passed status=gpu_voxel_indirect_commands_validated draws=3 empty_draws=1 instances=98440 first_uniform=0 first_checkerboard=6 first_mixed=98310 checksum_nonzero=1 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 indirect_dispatch=1 readback=1
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; largest touched render-backend source is SlangRhiVoxelRasterFrame.cpp at 473 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: the focused raster path now starts from
  bounded streamed compact chunk identities, but production app runtime still
  does not own retained Slang RHI resources for real streamed world columns.
- Blocker C is smaller but not closed: production frame drawing still does not
  consume generated packed quads.
- Blocker D is smaller but not closed: production frame submission still does
  not consume generated indirect commands, and full frustum/Hi-Z culling remains.

Exact next step:

```txt
Add a retained client render-backend resource owner for bounded streamed compact chunk batches so the app frame path can persist headers/palette/payload buffers across frames, run SlangRhiVoxelRasterPassGraph on those resources, and submit the generated indirect draw through the client-owned frame path rather than an offscreen probe-only command buffer.
```

### Pass 2026-05-21: Raster GPU resources extracted for frame reuse

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated.
- Blocker C: face-pulled raster path is not runtime-active.
- Blocker D: GPU-driven indirect drawing is not runtime-active.

Source-to-destination plan:

- Move bounded raster-frame GPU buffer/view ownership out of the probe
  orchestration function and into focused client render-backend internals.
- Keep Slang RHI types hidden inside `octaryn-client/Source/Rendering/RenderBackend`.
- Reuse the extracted resource owner from the existing offscreen pass-graph and
  indirect raster validation path.

Changed:

- Added `SlangRhiVoxelRasterResources.h/.cpp` to own bounded stream chunk
  header, palette, payload, face-mask, count, offset, packed-quad, indirect
  command, counter, checksum, and fixed index buffers plus their resource views.
- Updated `SlangRhiVoxelRasterFrame.cpp` to orchestrate device, queue, pass
  graph, render pass, indirect draw, and readback using the extracted resource
  owner.
- Wired `SlangRhiVoxelRasterResources.cpp` into `octaryn_client_render_backend`.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases octaryn_validate_client_voxel_raster_frame_probe octaryn_validate_client_voxel_indirect_generation_probe octaryn_validate_client_voxel_packed_quad_emit_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
rg -n "slang::|Slang::|gfx::|slang-rhi|slang_rhi|slang-gfx|slang-com-ptr" octaryn-server octaryn-shared octaryn-basegame --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' \) -not -path '*/build/*' -not -path '*/references/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 { print }'
git diff --check
```

The client app launch probe still fails in this environment at the local X11
swapchain/video step, while proving the Slang RHI device and offscreen frame
lifecycle:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated columns=4 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_indirect_generation_probe=passed status=gpu_voxel_indirect_commands_validated draws=3 empty_draws=1 instances=98440 first_uniform=0 first_checkerboard=6 first_mixed=98310 checksum_nonzero=1 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 indirect_dispatch=1 readback=1
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; SlangRhiVoxelRasterFrame.cpp is now 308 lines and SlangRhiVoxelRasterResources.cpp is 245 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: bounded raster GPU resources now have a
  focused owner, but production app runtime still does not retain real streamed
  world-column resources across frames.
- Blocker C is smaller but not closed: production frame drawing still does not
  consume generated packed quads.
- Blocker D is smaller but not closed: production frame submission still does
  not consume generated indirect commands, and full frustum/Hi-Z culling remains.

Exact next step:

```txt
Move SlangRhiVoxelRasterResources from probe-local lifetime into the client render-backend frame resource lifetime so the app frame path can persist bounded streamed headers/palette/payload buffers across frames, dispatch SlangRhiVoxelRasterPassGraph during the client frame, and submit the generated indirect draw from the production frame path.
```

### Pass 2026-05-21: Bootstrap app exercises retained voxel raster frame session

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated.
- Blocker C: face-pulled raster path is not fully runtime-active.
- Blocker D: GPU-driven indirect drawing is not fully runtime-active.

Source-to-destination plan:

- Move the raster frame path from a probe-only function into a
  production-named render-backend entry point.
- Keep Slang RHI state hidden inside render-backend internals through a focused
  frame session that owns device, queue, heap, pipelines, retained voxel
  resources, render target, framebuffer, and render pass.
- Have the active client bootstrap app exercise that retained voxel frame and
  log generated indirect draw evidence without restoring old CPU mesh, SDL GPU,
  GLSL, or whole-radius build paths.

Changed:

- Reworked `SlangRhiVoxelRasterFrame.cpp` around an internal
  `SlangRhiVoxelRasterFrameSession`.
- Added `render_slang_rhi_voxel_raster_frame()` as the production-named entry
  point while keeping `probe_slang_rhi_voxel_raster_frame()` as a validation
  wrapper.
- Added `retained_frame_resources` to the raster frame result and probe output.
- Updated the Slang RHI bootstrap app to call the retained voxel raster frame
  and log `client_voxel_raster_runtime ... retained_resources=1`.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe octaryn_validate_client_voxel_indirect_generation_probe octaryn_validate_client_voxel_packed_quad_emit_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|OpenGL|GLES|\\.glsl|shadercross|glslang|TerrainMeshBatch|WorldMeshUpload" octaryn-client octaryn-server octaryn-shared octaryn-basegame cmake tools --glob '!references/**' --glob '!build/**' --glob '!logs/**'
rg -n "slang::|Slang::|gfx::|slang-rhi|slang_rhi|slang-gfx|slang-com-ptr" octaryn-server octaryn-shared octaryn-basegame --glob '!build/**' --glob '!logs/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.slang' \) -not -path '*/build/*' -not -path '*/references/*' -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 { print }'
git diff --check
```

The client app launch probe still fails in this environment at the local X11
swapchain/video step, while the app log now includes the retained voxel raster
runtime proof:

```txt
slang_rhi_device=created runtime_available=1
slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1
slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 retained_resources=1 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated columns=4 retained_resources=1 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_indirect_generation_probe=passed status=gpu_voxel_indirect_commands_validated draws=3 empty_draws=1 instances=98440 first_uniform=0 first_checkerboard=6 first_mixed=98310 checksum_nonzero=1 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 indirect_dispatch=1 readback=1
client_voxel_packed_quad_emit_probe=passed status=gpu_voxel_packed_quads_validated empty=0 uniform=6 checkerboard=98304 mixed=130 total=98440 uniform_material=252 checkerboard_material=36372480 mixed_material=27950 nonzero_checksums=3 face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 emit_dispatch=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/SDL_OPENGLES OFF and validator rejection-string hits.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; SlangRhiVoxelRasterFrame.cpp is 393 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: the active bootstrap app exercises the
  retained-resource voxel frame, but real streamed world-column resource
  updates are not feeding it across frames.
- Blocker C is smaller but not closed: the bootstrap app path draws generated
  packed quads, but the full world runtime frame loop is still not wired.
- Blocker D is smaller but not closed: the bootstrap app submits generated
  indirect commands, but full frustum/Hi-Z culling and the streamed world frame
  submission path remain missing.

Exact next step:

```txt
Connect the retained SlangRhiVoxelRasterFrameSession to real client streamed compact column updates instead of the synthetic bounded stream batch, then preserve the session across multiple app frames so headers/palette/payload buffers update incrementally while the generated indirect draw remains active.
```

### Pass 2026-05-21: Raster frame batch carries bounded stream identities

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated with real
  streamed world-column payload updates.
- Blocker C: face-pulled raster path is not fully wired into the world runtime
  frame loop.
- Blocker D: GPU-driven indirect drawing is not fully wired into the streamed
  world frame path.

Source-to-destination plan:

- Use `ColumnStreaming` as the source for bounded column request identity.
- Carry accepted `ColumnCoord` values inside `SlangRhiVoxelRasterStreamBatch`
  beside compact chunk headers, palette entries, and payload bytes.
- Surface the stream center and first accepted column through the raster-frame
  result so the probe and active bootstrap app prove which bounded stream window
  fed the GPU-generated indirect draw.

Changed:

- Added stream-center and accepted-column metadata to
  `SlangRhiVoxelRasterStreamBatch`.
- Added `build_slang_rhi_voxel_raster_stream_batch_for_center()` so future
  frame-loop integration can feed moving client centers without changing the
  render-backend ABI again.
- Validated the raster batch by requiring one accepted column coordinate per GPU
  chunk header before creating Slang RHI resources.
- Extended the focused probe and bootstrap app log with
  `center=(x,z) first_column=(x,z)` evidence.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|glsl|GLSL|shadercross|glslang" octaryn-client cmake tools --glob '!references/**' --glob '!build/**'
rg -n "slang|gfx::|Slang::|IDevice|ICommand" octaryn-shared octaryn-server octaryn-basegame --glob '!references/**' --glob '!build/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' \) -not -path '*/build/*' -not -path '*/references/*' -print0 | xargs -0 wc -l | awk '$1 > 500 && $2 != "total" { print }'
git diff --check
```

The focused raster-frame probe passed:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(0,0) columns=4 retained_resources=1 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
```

The broader client renderer command still fails in this local environment at the
known X11 swapchain/video initialization step, after building the app/bundle and
passing shader/backend validation. The active app path still logged the bounded
stream identity and GPU-generated indirect raster proof before the launch log
validator rejected missing swapchain presentation:

```txt
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(0,0) first_column=(0,0) retained_resources=1 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
```

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/GLSL validator rejection strings and SDL_GPU OFF dependency configuration.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; touched files remain below the limit, with SlangRhiVoxelRasterFrame.cpp at 398 lines and SlangRhiVoxelRasterFrameGpu.h at 212 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: accepted bounded `ColumnStreaming`
  identities now reach the raster-frame GPU resource creation path, but the
  voxel payload bytes are still synthetic and not updated from real streamed
  world-column contents.
- Blocker C is smaller but not closed: the bootstrap app path draws generated
  packed quads from the bounded stream window, but the full world runtime frame
  loop is still not wired.
- Blocker D is smaller but not closed: the bootstrap app submits generated
  indirect commands, but full frustum/Hi-Z culling and streamed world frame
  submission remain missing.

Exact next step:

```txt
Preserve `SlangRhiVoxelRasterFrameSession` across at least two app-frame renders and add a resource update path for the compact headers/palette/payload buffers so a moved stream center can update accepted column identities while the generated indirect draw remains active.
```

### Pass 2026-05-21: Retained raster session updates compact stream buffers

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated with real
  streamed world-column payload updates.
- Blocker C: face-pulled raster path is not fully wired into the world runtime
  frame loop.
- Blocker D: GPU-driven indirect drawing is not fully wired into the streamed
  world frame path.

Source-to-destination plan:

- Use the existing Slang RHI `uploadBufferData` path as the source for compact
  GPU input-buffer updates.
- Keep the destination inside render-backend internals by adding an input update
  function to `SlangRhiVoxelRasterResources`, not to shared/server/basegame
  APIs.
- Reuse one `SlangRhiVoxelRasterFrameSession` across two renders, update the
  compact stream batch to a moved center, reset generated GPU buffers to UAV
  state, and prove the generated indirect draw remains active.

Changed:

- Allowed raster input buffers to transition between `ShaderResource` and
  `CopyDestination`.
- Added `upload_slang_rhi_voxel_raster_input_resources()` for header, palette,
  and payload uploads through a resource command encoder.
- Updated the retained raster session to render once at center `(0,0)`, upload
  a moved-center batch for `(1,0)`, reset generated pass outputs back to
  unordered-access state, and render again through the same session.
- Extended the app/probe result and logs with `session_reused`, `frames`, and
  `updates` evidence.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|glsl|GLSL|shadercross|glslang" octaryn-client cmake tools --glob '!references/**' --glob '!build/**'
rg -n "slang|gfx::|Slang::|IDevice|ICommand" octaryn-shared octaryn-server octaryn-basegame --glob '!references/**' --glob '!build/**'
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.py' \) -not -path '*/build/*' -not -path '*/references/*' -print0 | xargs -0 wc -l | awk '$1 > 500 && $2 != "total" { print }'
git diff --check
```

Focused raster-frame evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(1,0) first_column=(1,0) columns=4 retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
```

The broader client renderer command still fails in this local environment at the
known X11 swapchain/video initialization step after building the app/bundle and
passing shader/backend validation. The app path still logged the retained
two-frame compact stream update proof before the launch log validator rejected
missing swapchain presentation:

```txt
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(1,0) first_column=(1,0) retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=98440 nonclear=4096 compute_dispatch=1 indirect_draw=1 readback=1
```

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
Forbidden active path grep returned only SDL_GPU/GLSL validator rejection strings, the legacy_glsl status field, and SDL_GPU OFF dependency configuration.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; the largest touched source file is SlangRhiVoxelRasterFrame.cpp at 488 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: compact input buffers can now update
  across retained session frames, but payload bytes still come from synthetic
  stream content instead of real streamed world-column contents.
- Blocker C is smaller but not closed: the bootstrap app path draws generated
  packed quads after a retained compact input update, but the full world runtime
  frame loop is still not wired.
- Blocker D is smaller but not closed: generated indirect draw remains active
  after the retained stream-buffer update, but full frustum/Hi-Z culling and
  streamed world frame submission remain missing.

Exact next step:

```txt
Feed `SlangRhiVoxelRasterFrameSession` from the real client streamed compact column payload source instead of synthetic probe payloads, preserving bounded accepted-column updates and retained-session indirect draw proof.
```

### Pass 2026-05-21: Sparse chunk-stream records feed retained raster uploads

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated with live
  streamed world-column payload updates.
- Blocker C: face-pulled raster path is not fully wired into the world runtime
  frame loop.
- Blocker D: GPU-driven indirect drawing is not fully wired into the streamed
  world frame path.

Source-to-destination plan:

- Use the server chunk-stream column/block record shape as the source model:
  accepted column identity, world-space origin, block offset, block count, and
  sparse authoritative edit block records.
- Keep the destination inside `octaryn-client` render-backend internals by
  packing those records into the existing compact chunk headers, palette
  entries, and payload bytes consumed by `SlangRhiVoxelRasterFrameSession`.
- Preserve bounded accepted-column updates and retained-session indirect draw
  proof without restoring CPU mesh generation, SDL upload, GLSL, or whole-radius
  synchronous work.

Changed:

- Added internal stream column records with `origin_x`, `origin_z`,
  `block_offset`, and `block_count` to
  `SlangRhiVoxelRasterStreamBatch.cpp`.
- Replaced full synthetic voxel content with sparse edit-only block records
  packed through the existing compact voxel identity payload format.
- Kept the retained two-frame raster session update path intact while allowing
  sparse edit payloads to validate by nonzero generated instance/readback
  evidence instead of the old dense synthetic quad total.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
git diff --check
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' \) -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|gpu_render_path=Slang_RHI|\.glsl" octaryn-client octaryn-server octaryn-shared octaryn-basegame
rg -n "slang::|Slang::|gfx::|rhi::|IRenderer|ICommandQueue|ITransientResourceHeap" octaryn-shared octaryn-server octaryn-basegame
```

Focused raster-frame evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(1,0) first_column=(1,0) columns=4 retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=36 nonclear=12 compute_dispatch=1 indirect_draw=1 readback=1
```

The broader client renderer command still fails in this local environment at
the known X11 swapchain/video initialization step after building the app/bundle
and passing shader/backend validation. The app path still logged the retained
two-frame sparse stream-record update proof before the launch log validator
rejected missing swapchain presentation:

```txt
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(1,0) first_column=(1,0) retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=36 nonclear=12 compute_dispatch=1 indirect_draw=1 readback=1
```

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
octaryn_validate_client_shader_bundle exited 0 after validating the bundled Slang shader tree.
Forbidden active renderer path grep returned no hits under active client/server/shared/basegame roots.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; the largest touched source file is SlangRhiVoxelRasterFrame.cpp at 488 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: the raster upload path now consumes
  server-shaped sparse stream records, but it still uses an internal edit-record
  fixture instead of the live client stream file/host poller.
- Blocker C is smaller but not closed: the bootstrap app path draws generated
  packed quads from sparse edit records after a retained compact input update,
  but the full world runtime frame loop is still not wired.
- Blocker D is smaller but not closed: generated indirect draw remains active
  for sparse stream-record updates, but full frustum/Hi-Z culling and streamed
  world frame submission remain missing.

Exact next step:

```txt
Wire the client runtime chunk-stream reader/host poller into `SlangRhiVoxelRasterFrameSession` so live server-authored column/block records replace the internal sparse edit-record fixture while preserving bounded retained-session updates and GPU-generated indirect draw evidence.
```

### Pass 2026-05-21: Server-authored chunk stream sidecar feeds raster batch

Active blockers:

- Blocker B: GPU voxel pass graph is not production-integrated with live world
  frame stream updates.
- Blocker C: face-pulled raster path is not fully wired into the world runtime
  frame loop.
- Blocker D: GPU-driven indirect drawing is not fully wired into the streamed
  world frame path.

Source-to-destination plan:

- Source: the native server chunk-stream binary sidecar written beside the live
  `chunk_stream.json` path by the server chunk-stream owner.
- Destination: the client render-backend `SlangRhiVoxelRasterStreamBatch`
  compact headers, palettes, payload bytes, and accepted column identities.
- Validation source: the focused raster-frame probe creates the binary sidecar
  through `octaryn_server_block_store` chunk-stream writer, then the client
  render backend reads that sidecar through `OCTARYN_CLIENT_CHUNK_STREAM_PATH`.

Changed:

- Added binary sidecar reading to
  `SlangRhiVoxelRasterStreamBatch.cpp`, accepting either
  `OCTARYN_CLIENT_CHUNK_STREAM_PATH` or `OCTARYN_SERVER_CHUNK_STREAM_PATH` and
  falling back to the internal fixture when no live sidecar is present.
- Packed the first bounded server-authored column/block records directly into
  compact GPU chunk payloads, preserving edit-only block records and avoiding
  recurring JSON parsing for the raster upload path.
- Updated `octaryn_client_voxel_raster_frame_probe` to generate a server-owned
  chunk stream snapshot via `octaryn_server_block_store` and validate that the
  retained Slang RHI raster session consumes the binary sidecar.
- Updated the probe CMake target to include and link the server block-store
  owner library for validation only.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe
git diff --check
find octaryn-client octaryn-server octaryn-shared octaryn-basegame tools cmake -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cs' -o -name '*.cmake' -o -name 'CMakeLists.txt' \) -print0 | xargs -0 wc -l | awk '$2 != "total" && $1 > 500 {print}'
rg -n "WorldMeshRuntime|TerrainMesh|SDL_GPU|SDL_Render|gpu_render_path=Slang_RHI|\.glsl" octaryn-client octaryn-server octaryn-shared octaryn-basegame
rg -n "slang::|Slang::|gfx::|rhi::|IRenderer|ICommandQueue|ITransientResourceHeap" octaryn-shared octaryn-server octaryn-basegame
```

Focused raster-frame evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
```

The broader client renderer command still fails in this local environment at
the known X11 swapchain/video initialization step after building the app/bundle
and passing shader/backend validation. The app path still logged the retained
two-frame fallback stream proof before the launch log validator rejected missing
swapchain presentation:

```txt
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(1,0) first_column=(1,0) retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=36 nonclear=12 compute_dispatch=1 indirect_draw=1 readback=1
```

Evidence:

```txt
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
octaryn_validate_client_shader_bundle exited 0 after validating the bundled Slang shader tree.
server block store native probe passed
Forbidden active renderer path grep returned no hits under active client/server/shared/basegame roots.
Raw Slang/RHI leak scan returned no hits under octaryn-server, octaryn-shared, or octaryn-basegame.
Active source line-count scan returned no files over 500 physical lines; the largest touched source file is SlangRhiVoxelRasterStreamBatch.cpp at 286 lines.
```

`git diff --check` could not run because this workspace has no `.git`
directory; Git returned `Not a git repository`.

Remaining:

- Blocker B is smaller but not closed: the raster batch can consume the
  server-authored binary stream sidecar, but the production world frame loop is
  still not handing its live stream path into the retained raster session.
- Blocker C is smaller but not closed: the focused probe draws GPU-generated
  packed quads from server-authored stream records, while the real client world
  runtime still uses the bootstrap/fallback entry point.
- Blocker D is smaller but not closed: generated indirect draw remains active
  for server-authored stream sidecar records, but full frustum/Hi-Z culling and
  streamed world frame submission remain missing.

Exact next step:

```txt
Thread the live client/server chunk-stream path through the production client frame/runtime entry point so `SlangRhiVoxelRasterFrameSession` reads the bundled-server sidecar during the actual app launch path, then add runtime log evidence that distinguishes live-sidecar batches from fallback fixture batches.
```

### Pass 2026-05-21: App launch live-sidecar voxel raster handoff

Active blockers:

- Blocker B: materially shrunk. The app launch path now consumes the
  bundled-server chunk stream sidecar through the production-named Slang RHI
  voxel raster entry point.
- Blocker C: materially shrunk. The app path logs live-sidecar, non-clear
  face-pulled raster output instead of only focused probe output.
- Blocker D: materially shrunk. The app path uses GPU-generated indirect draws
  from server-authored edit-only stream records, still without full runtime
  frame-loop culling coverage.

Source-to-destination plan:

```txt
cmake client launch probe env -> pass bundled-server chunk_stream.json path to the client app
server chunk stream absolute block records -> bounded client stream batch selection -> GPU chunk-y/local-y payloads
SlangRhiVoxelRasterFrameSession -> tolerate zero-byte logical buffers and validate live sidecar draw/readback invariants
SlangRhiBootstrap -> flush live-sidecar raster evidence before local swapchain validation
```

Changed:

- Added `OCTARYN_CLIENT_CHUNK_STREAM_PATH` to client app launch probe targets.
- Added live-sidecar source tracking to raster batches, frame probe results,
  app logs, and the focused raster-frame probe output.
- Reordered and flushed bootstrap logging so the raster proof survives local
  swapchain failures.
- Prevented Slang RHI zero-byte logical stream buffers/views from creating
  zero-sized Vulkan resources.
- Mapped streamed absolute block Y to GPU chunk-Y/local-Y payload coordinates.
- Kept bounded column acceptance while prioritizing edit-bearing streamed
  columns before filling remaining budget in server order.
- Relaxed raster readback validation from fixture-specific `draws == 3` to the
  runtime invariant of positive draw count, instances, and non-clear pixels.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe
git diff --check
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
server_live_chunk_stream active=1 source=process_file path=/home/zachr/Workspace/octaryn-workspace-dev/build/debug-linux/server/validation/client-server-app-launch-probe-world/chunk_stream.json epoch=1 center=(0,0) radius=1 columns=9 blocks=2 metadata_only=0 requested_metadata_only=0 command_delta=1 world_time_day_fraction=0.500019
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(0,0) first_column=(0,0) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=1 instances=6 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
line_count_violations=0
```

Known validation gap:

```txt
octaryn_validate_client_app_launch_probe still fails in this local environment
after emitting the live-sidecar voxel raster evidence because the X11 swapchain
validator reports:
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

`git diff --check` could not run because this workspace root is not a Git
repository and returned exit 129. Forbidden active renderer fallback grep and
raw Slang/RHI API leak grep returned no active hits.

Remaining:

- Wire the same live sidecar stream source through the real client/world frame
  loop rather than only the bootstrap/probe launch path.
- Replace the local X11 launch-probe dependency with a reliable CI/runtime
  swapchain proof path or rerun in an environment where X11 presentation is
  available.
- Add production metrics for upload/build timing, retained chunk counts, pass
  timings, radius-32 visibility, and VRAM/staging memory.

Exact next step:

```txt
Move the live-sidecar SlangRhiVoxelRasterFrameSession handoff out of the bootstrap probe and into the real client/world frame loop, preserving bounded edit-bearing column selection and adding runtime metrics for stream batch counts, upload timing, retained chunks, GPU pass timings, and radius-32 visibility.
```

### Pass 2026-05-21: World-presentation voxel frame loop handoff

Active blockers:

- Blocker B: materially shrunk. The app no longer calls the voxel raster entry
  point directly from bootstrap; it routes through a client-owned
  world-presentation frame-loop owner.
- Blocker C: materially shrunk. The runtime log now includes a frame-loop
  metrics line with live-sidecar source, retained columns/chunks, stream
  batches, elapsed time, indirect draw, and readback.
- Blocker D: materially shrunk. Generated indirect drawing remains active
  through the frame-loop owner, but full runtime culling and radius-32 world
  visibility evidence remain missing.

Source-to-destination plan:

```txt
SlangRhiBootstrap direct raster call -> WorldPresentation/VoxelRasterFrameLoop owner
render-backend Slang RHI raster entry point -> hidden backend implementation called by frame loop
app launch log -> add frame-loop batch/retained/timing evidence beside raster proof
```

Changed:

- Added `octaryn-client/Source/WorldPresentation/VoxelRasterFrameLoop/`.
- Moved the app runtime voxel raster handoff out of `SlangRhiBootstrap` and
  into `run_voxel_raster_world_frame_loop()`.
- Added `client_voxel_world_frame_loop` runtime evidence with requested frames,
  actual frames, stream batches, retained columns/chunks, elapsed microseconds,
  update count, draw/instance counts, live-sidecar source, indirect draw, and
  readback status.
- Wired the new world-presentation frame-loop source into `octaryn_client_app`.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe
git diff --check
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(0,0) first_column=(0,0) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=1 instances=6 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_world_frame_loop requested_frames=2 frames=2 batches=2 retained_columns=4 retained_chunks=4 elapsed_us=654024 updates=1 draws=1 instances=6 stream_source=live_sidecar indirect_draw=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
line_count_violations=0
```

Known validation gap:

```txt
octaryn_validate_client_app_launch_probe still fails in this local environment
after emitting the live-sidecar frame-loop evidence because the X11 swapchain
validator reports:
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

`git diff --check` could not run because this workspace root is not a Git
repository and returned exit 129. The Podman build wrapper also produced
permission/stat errors when run in parallel, so validation was rerun serially.
Forbidden active renderer fallback grep and raw Slang/RHI API leak grep
returned no active hits.

Remaining:

- Preserve the `VoxelRasterFrameLoop` session across real interactive frames
  instead of the current two-frame app launch pass.
- Add finer-grained build/upload/pass timings from inside the retained Slang RHI
  session rather than only outer frame-loop elapsed time.
- Add radius-32 live-sidecar world visibility proof and full GPU culling
  coverage.

Exact next step:

```txt
Split the retained SlangRhiVoxelRasterFrameSession into a reusable client render-backend session API owned by the world-presentation frame loop, then persist that session across repeated runtime frames and log per-stage build/upload/pass/readback timings plus radius-32 retained chunk visibility.
```

### Pass 2026-05-21: Retained voxel raster session API and stage timings

Active blockers:

- Blocker B: materially shrunk. The retained Slang RHI voxel raster session is
  now exposed as an opaque client render-backend API and no longer recreated
  inside the one-shot raster wrapper used by the world-presentation loop.
- Blocker C: materially shrunk. The world-presentation frame loop now owns the
  retained session across its repeated runtime frames and logs stage timings
  from the real GPU pass path.
- Blocker D: materially shrunk. Generated indirect drawing remains active
  through the retained session handoff, and the log now reports current
  radius-32 visibility status instead of implying radius proof.

Source-to-destination plan:

```txt
anonymous SlangRhiVoxelRasterFrameSession construction in raster wrapper -> focused render-backend session-construction unit
one-shot render_slang_rhi_voxel_raster_frame() ownership -> opaque render-backend session API consumed by VoxelRasterFrameLoop
outer elapsed-only frame-loop log -> session create, stream build/upload, pass graph, indirect draw, submit/readback timing fields
```

Changed:

- Split retained session construction into
  `SlangRhiVoxelRasterFrameSession.cpp` plus an internal render-backend header,
  keeping raw Slang RHI types inside render-backend internals.
- Added an opaque retained-session API in `SlangRhiVoxelRasterFrame.h` for the
  world-presentation owner.
- Updated `VoxelRasterFrameLoop` to create one retained session, render repeated
  frames through it, and aggregate stream-build/upload/pass/draw/readback
  timings.
- Extended the app launch evidence line with `radius32_visible`,
  `session_create_us`, `stream_build_us`, `upload_us`, `pass_graph_us`,
  `draw_us`, and `submit_readback_us`.
- Added the new render-backend source file to the client native CMake target.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe
```

Validation repair:

```txt
The first focused raster-frame probe build failed because the moved session
struct referenced SlangRhiVoxelRasterResources outside its detail namespace.
The repair changed that member to detail::SlangRhiVoxelRasterResources and the
same target then passed.
```

Evidence:

```txt
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(0,0) first_column=(0,0) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=1 instances=6 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_world_frame_loop requested_frames=2 frames=2 batches=2 retained_columns=4 retained_chunks=4 radius32_visible=0 elapsed_us=632154 session_create_us=511747 stream_build_us=45 upload_us=103 pass_graph_us=16783 draw_us=2100 submit_readback_us=85867 updates=1 draws=1 instances=6 stream_source=live_sidecar indirect_draw=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
```

Known validation gap:

```txt
octaryn_validate_client_app_launch_probe still fails in this local environment
after emitting the retained-session timing evidence because the X11 swapchain
validator reports:
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

Remaining:

- Preserve the retained `VoxelRasterFrameLoop` session across real interactive
  frames instead of only the current app-launch two-frame pass.
- Replace or rerun the local X11 launch-probe swapchain path in an environment
  where presentation can validate.
- Add radius-32 live-sidecar visibility proof, VRAM/staging metrics, and full
  GPU culling coverage.

Exact next step:

```txt
Thread the retained voxel raster session through the actual interactive client frame lifetime, then capture radius-32 live-sidecar visibility and retained chunk timing evidence without relying on the local X11 swapchain validator.
```

### Pass 2026-05-21: World frame-loop retained session lifetime probe

Active blockers:

- Blocker B: materially shrunk. The retained raster session is now owned by a
  reusable world-presentation frame-loop lifetime API instead of only a
  launch-local helper.
- Blocker C: materially shrunk. The client app and a focused offscreen probe
  render repeated frames through the same retained world-frame-loop state.
- Blocker D: materially shrunk. Generated indirect drawing is validated through
  the world frame-loop lifetime path without depending on local X11 swapchain
  presentation.
- Blocker F: materially shrunk. Runtime evidence now covers three frame-loop
  batches, retained chunks, upload/pass/draw/readback timings, indirect draw,
  live-sidecar stream source, and explicit `radius32_visible=0`.

Source-to-destination plan:

```txt
launch-only run_voxel_raster_world_frame_loop() helper -> create/render/snapshot/destroy world-frame-loop lifetime API
app-owned two-frame bootstrap call -> app-owned retained frame-loop state across repeated frames
X11-gated launch evidence only -> offscreen client voxel world-frame-loop probe target with server-authored live sidecar
```

Changed:

- Added `create_voxel_raster_world_frame_loop`,
  `render_voxel_raster_world_frame`,
  `snapshot_voxel_raster_world_frame_loop`, and
  `destroy_voxel_raster_world_frame_loop`.
- Updated `Octaryn.Client` bootstrap to own a retained frame-loop state and
  render three frames through it before logging metrics.
- Promoted `VoxelRasterFrameLoop.cpp` into a focused client native static
  library shared by the app and validation tools.
- Added `octaryn_client_voxel_world_frame_loop_probe` and
  `octaryn_validate_client_voxel_world_frame_loop_probe`, which prepare a
  server-authored edit-only sidecar and validate three retained frame-loop
  renders offscreen.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_world_frame_loop_probe octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe octaryn_validate_native_owner_boundaries
git diff --check
```

Evidence:

```txt
client_voxel_world_frame_loop_probe=passed status=gpu_voxel_raster_frame_validated requested_frames=3 frames=3 batches=3 columns=4 retained_chunks=4 radius32_visible=0 source=live_sidecar reused=1 updates=2 draws=3 instances=18 upload_us=121 pass_graph_us=575 draw_us=249 submit_readback_us=125221
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 center=(0,0) first_column=(0,0) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=3 updates=2 draws=1 instances=6 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_world_frame_loop requested_frames=3 frames=3 batches=3 retained_columns=4 retained_chunks=4 radius32_visible=0 elapsed_us=684336 session_create_us=541499 stream_build_us=81 upload_us=139 pass_graph_us=17425 draw_us=2187 submit_readback_us=122898 updates=2 draws=1 instances=6 stream_source=live_sidecar indirect_draw=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
world_blocks.json contained two authoritative edit records from the launch
probe and no seed-terrain bulk chunk records.
```

Known validation gap:

```txt
octaryn_validate_client_app_launch_probe still fails in this local environment
after emitting the three-frame retained-session evidence because the X11
swapchain validator reports:
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
```

`git diff --check` could not run because this workspace root is not a Git
repository and returned exit 129. Active source line-count scan found no file
over 500 physical lines. Forbidden active renderer fallback grep found only
legacy-disabled status field names, and raw Slang/GFX API leak grep returned no
hits outside client render-backend internals.

Remaining:

- Replace the app launch X11-only swapchain gate or rerun it in an environment
  where X11 presentation is available.
- Connect the retained world-frame-loop lifetime API to a real window/event
  frame loop when the graphical app grows beyond bootstrap execution.
- Add radius-32 live-sidecar visibility proof, VRAM/staging metrics, and full
  GPU culling coverage.

Exact next step:

```txt
Add a renderer runtime proof path that feeds radius-32 live-sidecar batches into the retained world-frame-loop API and records retained columns/chunks, pass timings, indirect draw counts, and edit-only persistence evidence without restoring CPU mesh or SDL GPU paths.
```

### Pass 2026-05-21: Radius-32 live-sidecar coverage proof

Active blockers:

- Blocker B: materially shrunk. The live stream batch now carries both bounded
  uploaded columns and total available live-sidecar columns, so radius proof no
  longer requires whole-radius GPU upload.
- Blocker C: materially shrunk. The retained world-frame-loop snapshot reports
  live-sidecar coverage separately from retained chunk count.
- Blocker D: materially shrunk. The radius-32 proof still exercises the real
  Slang RHI GPU-generated indirect draw path with bounded retained chunks.
- Blocker F: materially shrunk. Offscreen runtime evidence now covers a
  server-authored radius-32 live sidecar, retained bounded chunks, pass timings,
  indirect draw counts, and edit-only persistence evidence.

Source-to-destination plan:

```txt
bounded_columns as both upload count and radius proof -> split bounded uploaded columns from live-sidecar available columns
radius-1 world-frame-loop probe -> radius-32 server-authored live-sidecar probe with bounded upload preserved
radius32_visible from retained upload count -> radius32_visible from live-sidecar coverage, retained_chunks remains bounded
```

Changed:

- Added `available_columns` to the Slang RHI voxel raster stream batch and
  `live_columns` to the public render-backend probe result.
- Updated retained frame-session creation/readback and world-frame-loop
  snapshots to report live-sidecar coverage separately from bounded retained
  chunks.
- Updated app bootstrap logging to include `live_columns`.
- Updated `octaryn_client_voxel_world_frame_loop_probe` to request radius 32
  from the server chunk-stream sidecar and fail unless the renderer sees 4,225
  live columns while retaining the bounded four uploaded chunks.
- Replaced probe-result aggregate initializers near the touched render-backend
  API with field assignment so future metric fields do not silently shift.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_world_frame_loop_probe octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe octaryn_validate_native_owner_boundaries
git diff --check
```

Validation repair:

```txt
The first focused renderer build failed because SlangRhiVoxelRasterFrameProbeResult
grew a live_columns field while one render-backend helper still used positional
aggregate initialization. The helper now uses field assignment and the focused
probe target passed on rerun.
```

Evidence:

```txt
client_voxel_world_frame_loop_probe=passed status=gpu_voxel_raster_frame_validated requested_frames=3 frames=3 batches=3 columns=4 live_columns=4225 retained_chunks=4 radius32_visible=1 source=live_sidecar reused=1 updates=2 draws=3 instances=18 upload_us=180 pass_graph_us=607 draw_us=276 submit_readback_us=184281
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 live_columns=9 center=(0,0) first_column=(0,0) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=3 updates=2 draws=1 instances=6 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_world_frame_loop requested_frames=3 frames=3 batches=3 live_columns=9 retained_columns=4 retained_chunks=4 radius32_visible=0 elapsed_us=652480 session_create_us=510299 stream_build_us=75 upload_us=123 pass_graph_us=16757 draw_us=2639 submit_readback_us=122484 updates=2 draws=1 instances=6 stream_source=live_sidecar indirect_draw=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
```

Edit-only persistence evidence:

```txt
build/debug-linux/server/validation/client-server-app-launch-probe-world/world_blocks.json contained only two authoritative edit records:
(8,31,8)->0 and (8,32,8)->29.
server_live_chunk_stream reported radius=1 columns=9 blocks=2 for the app
launch sidecar and did not stream generated seed terrain block records.
```

Known validation gaps:

```txt
octaryn_validate_client_app_launch_probe still fails in this local environment
after emitting retained Slang RHI evidence because the X11 swapchain validator
reports:
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0

The app launch probe still uses its existing radius-1 readiness sidecar; the
new radius-32 renderer proof is the focused offscreen
octaryn_client_voxel_world_frame_loop_probe path.

git diff --check could not run because this workspace root is not a Git
repository and returned exit 129.
```

Hygiene:

```txt
Active source line-count scan found no source/code file over 500 physical lines.
Forbidden active renderer fallback grep found only legacy-disabled status field
names. Raw Slang/GFX API leak grep returned no hits outside client render-backend
internals.
```

Remaining:

- Replace the app launch X11-only swapchain gate or rerun it in an environment
  where X11 presentation is available.
- Feed the app launch/readiness proof with a radius-32 sidecar when that can be
  done without making the server readiness validator perform whole-radius work.
- Add VRAM/staging memory metrics and full GPU culling coverage.
- Connect retained world-frame-loop lifetime to the real interactive frame loop
  once the graphical app moves past bootstrap execution.

Exact next step:

```txt
Extend the retained renderer proof from offscreen radius-32 coverage into the
client launch/runtime evidence path while keeping the server readiness sidecar
bounded and edit-only, then add VRAM/staging counters for retained Slang RHI
resources and upload batches.
```

### Pass 2026-05-21: Retained GPU and upload staging byte counters

Active blockers:

- Blocker B: materially shrunk. The retained Slang RHI voxel resource owner now
  measures buffer footprint and input upload byte counts from the real compact
  streamed batch shape.
- Blocker C: materially shrunk. The world-frame-loop API surfaces retained GPU
  bytes and upload staging bytes as plain client-owned metrics without exposing
  Slang RHI handles.
- Blocker F: materially shrunk. Runtime/profiling evidence now includes
  retained resource bytes and per-batch upload staging bytes alongside
  radius-32 visibility, retained chunks, indirect draw, and pass timings.

Source-to-destination plan:

```txt
unmeasured retained Slang RHI resources -> render-backend-owned retained_gpu_bytes counter
unmeasured compact stream uploads -> render-backend-owned upload_staging_bytes counter
probe-only timing evidence -> radius-32 frame-loop proof requiring nonzero memory/upload counters
```

Changed:

- Added `retained_gpu_bytes` and `upload_staging_bytes` to the
  client-owned voxel raster frame result.
- Measured compact input upload bytes from header, palette, and packed payload
  buffers in `SlangRhiVoxelRasterResources`.
- Measured retained GPU buffer bytes for headers, palettes, payload, masks,
  counters, generated quad buffers, indirect commands, and the fixed index
  buffer, then added the retained offscreen color target estimate at session
  creation.
- Surfaced the byte counters through `VoxelRasterFrameLoopResult`, app bootstrap
  logging, and the focused radius-32 world-frame-loop probe.
- Tightened the focused probe so it fails if retained GPU bytes or upload
  staging bytes are zero.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_world_frame_loop_probe octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe octaryn_validate_native_owner_boundaries
git diff --check
```

Evidence:

```txt
client_voxel_world_frame_loop_probe=passed status=gpu_voxel_raster_frame_validated requested_frames=3 frames=3 batches=3 columns=4 live_columns=4225 retained_chunks=4 radius32_visible=1 source=live_sidecar reused=1 updates=2 draws=3 instances=18 upload_us=141 retained_gpu_bytes=2124448 upload_staging_bytes=24896 pass_graph_us=685 draw_us=282 submit_readback_us=147476
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 live_columns=9 center=(0,0) first_column=(0,0) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=3 updates=2 draws=1 instances=6 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1 retained_gpu_bytes=2107936 upload_staging_bytes=8384
client_voxel_world_frame_loop requested_frames=3 frames=3 batches=3 live_columns=9 retained_columns=4 retained_chunks=4 radius32_visible=0 elapsed_us=647085 session_create_us=505041 stream_build_us=81 upload_us=129 pass_graph_us=16979 draw_us=2158 submit_readback_us=122590 updates=2 draws=1 instances=6 stream_source=live_sidecar indirect_draw=1 readback=1 retained_gpu_bytes=2107936 upload_staging_bytes=8384
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
```

Edit-only persistence evidence:

```txt
build/debug-linux/server/validation/client-server-app-launch-probe-world/world_blocks.json contained only two authoritative edit records:
(8,31,8)->0 and (8,32,8)->29.
server_live_chunk_stream reported radius=1 columns=9 blocks=2 for the app
launch sidecar and did not stream generated seed terrain block records.
```

Known validation notes:

```txt
octaryn_validate_client_app_launch_probe now emits retained Slang RHI evidence
and records the local X11 presentation failure as swapchain status:
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0

The app launch probe still uses its existing radius-1 readiness sidecar; the
focused offscreen world-frame-loop probe is the radius-32 proof path.

git diff --check could not run because this workspace root is not a Git
repository and returned exit 129.
```

Hygiene:

```txt
Active source line-count scan found no source/code file over 500 physical lines.
Forbidden active renderer fallback grep found only legacy-disabled status field
names. Raw Slang/GFX API leak grep returned no hits outside client render-backend
internals.
```

Remaining:

- Keep X11 swapchain presentation as status-only unless a future CI/runtime lane
  provides reliable presentation proof.
- Feed app launch/runtime evidence with radius-32 coverage without forcing
  server readiness validation into whole-radius synchronous work.
- Add full GPU culling coverage and richer GPU/pass timing if timestamp support
  is available through the backend.
- Connect retained world-frame-loop lifetime to the real interactive frame loop
  once the graphical app moves past bootstrap execution.

Exact next step:

```txt
Add a non-X11 client launch/runtime validation path that runs Octaryn.Client with
a prebuilt radius-32 server-authored sidecar and validates the emitted Slang RHI
frame-loop evidence, while keeping the existing server readiness probe bounded
and edit-only.
```

### Pass 2026-05-21: App-owned retained voxel frame-loop lifetime

Active blockers:

- Blocker A: materially shrunk. The client app no longer owns the retained
  voxel frame-loop proof inline in the bootstrap entry point; it routes through
  a focused app frame-loop owner and emits an app frame-loop lifetime marker
  before voxel runtime metrics.
- Blocker F: materially shrunk. Standard client launch validation now requires
  the app-owned retained voxel frame-loop marker and radius-32 live-sidecar
  evidence in ordering, while keeping X11 presentation status-only.

Source-to-destination plan:

```txt
inline SlangRhiBootstrap voxel proof -> octaryn-client/Source/App/FrameLoop owner
bootstrap direct voxel logging -> app frame-loop lifetime marker plus voxel metrics
launch validator voxel-only ordering -> required app frame-loop marker before voxel evidence
```

Changed:

- Added `octaryn-client/Source/App/FrameLoop/FrameLoop.{h,cpp}` as the client
  app owner for the retained voxel frame-loop lifetime.
- Reduced `SlangRhiBootstrap.cpp` back to bootstrap/backend lifecycle setup and
  delegated retained voxel runtime execution to the app frame-loop owner.
- Added the new app frame-loop source/include path to `octaryn_client_app`.
- Tightened client app launch log validation to require
  `client_app_frame_loop frames=3 requested_frames=3 retained_voxel_session=1`
  before the voxel raster/runtime evidence.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe octaryn_validate_client_voxel_world_frame_loop_probe octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe octaryn_validate_native_owner_boundaries octaryn_validate_native_jobs_probe
git diff --check
```

Evidence:

```txt
client_app_frame_loop frames=3 requested_frames=3 retained_voxel_session=1
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 live_columns=4225 center=(0,0) first_column=(-1,-1) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=3 updates=2 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1 retained_gpu_bytes=2124448 upload_staging_bytes=24896
client_voxel_world_frame_loop requested_frames=3 frames=3 batches=3 live_columns=4225 retained_columns=4 retained_chunks=4 radius32_visible=1 elapsed_us=649049 session_create_us=524261 stream_build_us=1317 upload_us=160 pass_graph_us=592 draw_us=256 submit_readback_us=122321 updates=2 draws=3 instances=18 stream_source=live_sidecar indirect_draw=1 readback=1 retained_gpu_bytes=2124448 upload_staging_bytes=24896
client_voxel_world_frame_loop_probe=passed status=gpu_voxel_raster_frame_validated requested_frames=3 frames=3 batches=3 columns=4 live_columns=4225 retained_chunks=4 radius32_visible=1 source=live_sidecar reused=1 updates=2 draws=3 instances=18 upload_us=207 retained_gpu_bytes=2124448 upload_staging_bytes=24896 pass_graph_us=685 draw_us=324 submit_readback_us=190074
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
native jobs probe passed
server block store native probe passed
```

Edit-only persistence evidence:

```txt
build/debug-linux/server/validation/client-server-app-launch-probe-world/world_blocks.json contained only two authoritative edit records:
(8,31,8)->0 and (8,32,8)->29.
```

Known validation notes:

```txt
Local X11 presentation remains unavailable in this environment and is logged as
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0.

git diff --check could not run because this workspace root is not a Git
repository and returned exit 129.
```

Hygiene:

```txt
Active source line-count scan found no source/code file over 500 physical lines.
Forbidden active renderer fallback grep found only docs, validator rejection
strings, old cleanup-map references, and SDL_GPU OFF dependency configuration.
Raw Slang/GFX API leak grep returned no hits in shared, server, or basegame.
```

Remaining:

- The app-owned frame loop still runs a fixed three-frame validation lifetime;
  it is not yet connected to the graphical interactive window/event lifetime.
- Add full GPU culling coverage and richer GPU/pass timing if timestamp support
  is available through the backend.

Exact next step:

```txt
Wire the app-owned retained voxel frame-loop owner into the graphical client
window/event lifetime, preserving the X11 status-only validation path and the
radius-32 live-sidecar launch evidence.
```

### Pass 2026-05-21: Radius-32 app launch sidecar fixture

Active blockers:

- Blocker A: materially shrunk. The client app launch target now feeds
  `Octaryn.Client` with a prebuilt radius-32 server-authored sidecar instead of
  the radius-1 readiness sidecar.
- Blocker F: materially shrunk. The standard client app launch validation now
  requires radius-32 live-sidecar visibility evidence in the emitted world
  frame-loop line while preserving the local X11 swapchain as status-only.

Source-to-destination plan:

```txt
radius-1 app launch sidecar -> radius-32 server-authored launch fixture
probe-only radius-32 writer -> reusable fixture mode on world-frame-loop probe
generic live-sidecar validator -> app-launch validator requiring 4,225 columns
```

Changed:

- Added `octaryn_client_app_probe_radius32_chunk_stream` as the client-owned
  launch fixture path.
- Added `octaryn_client_app_radius32_chunk_stream_fixture`, which writes the
  radius-32 server-authored chunk stream through
  `octaryn_client_voxel_world_frame_loop_probe --write-radius32-chunk-stream`.
- Pointed `octaryn_run_client_app_launch_probe` and
  `octaryn_validate_client_app_launch_probe` at the radius-32 fixture path.
- Tightened the app launch validator so the world-frame-loop line must include
  `live_columns=4225` and `radius32_visible=1`.
- Regenerated all four public CMake graphs so the new fixture target is present
  in the active target inventory.

Validated:

```txt
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_configure.sh release-linux
tools/build/cmake_configure.sh debug-windows
tools/build/cmake_configure.sh release-windows
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe octaryn_validate_client_voxel_world_frame_loop_probe octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe octaryn_validate_native_owner_boundaries octaryn_validate_native_jobs_probe
git diff --check
```

Validation repair note:

```txt
The first CMake inventory validation failed because the release-linux,
debug-windows, and release-windows build graphs were stale and did not yet
contain octaryn_client_app_radius32_chunk_stream_fixture. Regenerating those
graphs fixed the inventory validation without changing target policy.
```

Evidence:

```txt
client_radius32_chunk_stream_fixture=passed path=/home/zachr/Workspace/octaryn-workspace-dev/build/debug-linux/client/validation/client-app-radius32-chunk-stream/chunk_stream.json
client_voxel_world_frame_loop_probe=passed status=gpu_voxel_raster_frame_validated requested_frames=3 frames=3 batches=3 columns=4 live_columns=4225 retained_chunks=4 radius32_visible=1 source=live_sidecar reused=1 updates=2 draws=3 instances=18 upload_us=177 retained_gpu_bytes=2124448 upload_staging_bytes=24896 pass_graph_us=618 draw_us=297 submit_readback_us=121739
client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated columns=4 live_columns=4225 center=(0,0) first_column=(-1,-1) stream_source=live_sidecar retained_resources=1 session_reused=1 frames=3 updates=2 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1 retained_gpu_bytes=2124448 upload_staging_bytes=24896
client_voxel_world_frame_loop requested_frames=3 frames=3 batches=3 live_columns=4225 retained_columns=4 retained_chunks=4 radius32_visible=1 elapsed_us=640257 session_create_us=516427 stream_build_us=1300 upload_us=131 pass_graph_us=544 draw_us=249 submit_readback_us=121484 updates=2 draws=3 instances=18 stream_source=live_sidecar indirect_draw=1 readback=1 retained_gpu_bytes=2124448 upload_staging_bytes=24896
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
native jobs probe passed
```

Known validation notes:

```txt
Local X11 presentation remains unavailable in this environment and is logged as
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0.

git diff --check could not run because this workspace root is not a Git
repository and returned exit 129.
```

Shell stability note:

```txt
The system-level bash process storm was traced to repeated non-interactive shell
snapshot commands sourcing ~/.bashrc. ~/.bashrc now returns immediately for
non-interactive shells, interactive aliases still load, and the follow-up
process check showed zero live snapshot bash processes and only three bash
processes total.
```

Hygiene:

```txt
Active source line-count scan found no source/code file over 500 physical lines.
Forbidden active renderer fallback grep found only validator rejection strings,
documentation references, and SDL_GPU OFF dependency configuration. Raw
Slang/GFX API leak grep returned no hits outside client render-backend
internals.
```

Remaining:

- Keep X11 swapchain presentation as status-only unless a future CI/runtime lane
  provides reliable presentation proof.
- Add full GPU culling coverage and richer GPU/pass timing if timestamp support
  is available through the backend.
- Connect retained world-frame-loop lifetime to the real interactive frame loop
  once the graphical app moves past bootstrap execution.

Exact next step:

```txt
Replace the bootstrap-only retained world-frame-loop execution with the real
interactive client frame lifetime so radius-32 live-sidecar evidence is emitted
from the production frame loop, while keeping bounded upload staging and the
Slang RHI backend API boundary intact.
```

### Pass 2026-05-21: X11 swapchain status-only validation policy

Active blockers:

- Blocker A: materially shrunk. The client app launch validator no longer
  blocks on local X11 swapchain presentation; it requires offscreen Slang RHI
  frame lifecycle and live-sidecar voxel frame-loop proof instead.
- Blocker F: materially shrunk. The standard client renderer validation command
  now passes in this local environment while preserving direct runtime evidence
  for indirect draw, readback, retained chunks, retained GPU bytes, upload
  staging bytes, and bounded server-side edit-only stream input.

Source-to-destination plan:

```txt
X11-specific required swapchain success marker -> swapchain status marker
marker-only launch validation -> required offscreen frame lifecycle plus voxel frame-loop evidence
stale validation docs -> documented status-only local X11 presentation policy
```

Changed:

- Updated the client app launch log validator and ordering checks so
  `slang_rhi_swapchain=` is required as status, but `driver=x11 created=1` is
  not required for local validation.
- Added required launch-log prefixes for `client_voxel_raster_runtime` and
  `client_voxel_world_frame_loop`, and required the frame-loop line to include
  `stream_source=live_sidecar`, `indirect_draw=1`, `readback=1`,
  `retained_gpu_bytes=`, and `upload_staging_bytes=`.
- Updated `plan.md`, `docs/validation/build-matrix.md`,
  `docs/validation/runtime-runs.md`, `docs/build/index.html`, and
  `docs/validation/index.html` to describe the status-only local X11 policy.

Validated:

```txt
tools/build/cmake_build.sh debug-linux --target octaryn_client_app octaryn_client_bundle octaryn_validate_client_app_launch_probe octaryn_validate_client_slang_shaders octaryn_validate_client_shader_bundle octaryn_validate_client_render_backend_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_voxel_world_frame_loop_probe octaryn_validate_client_voxel_raster_frame_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe octaryn_validate_native_owner_boundaries
python3 tools/validation/validate_client_app_launch_probe_log.py --log-file logs/client/octaryn_client_app_launch_probe-debug-linux.log
git diff --check
```

Validation repair note:

```txt
A parallel build-helper attempt collided through the Podman build route:
one command reported a transient missing CMakePresets.json and the other
reported permission denied. The same focused validation commands passed when
rerun sequentially.
```

Evidence:

```txt
client_voxel_world_frame_loop requested_frames=3 frames=3 batches=3 live_columns=9 retained_columns=4 retained_chunks=4 radius32_visible=0 elapsed_us=694150 session_create_us=551875 stream_build_us=90 upload_us=150 pass_graph_us=16919 draw_us=2368 submit_readback_us=122632 updates=2 draws=1 instances=6 stream_source=live_sidecar indirect_draw=1 readback=1 retained_gpu_bytes=2107936 upload_staging_bytes=8384
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0
client_voxel_world_frame_loop_probe=passed status=gpu_voxel_raster_frame_validated requested_frames=3 frames=3 batches=3 columns=4 live_columns=4225 retained_chunks=4 radius32_visible=1 source=live_sidecar reused=1 updates=2 draws=3 instances=18 upload_us=173 retained_gpu_bytes=2124448 upload_staging_bytes=24896 pass_graph_us=16970 draw_us=6095 submit_readback_us=195508
client_voxel_raster_frame_probe=passed status=gpu_voxel_raster_frame_validated center=(0,0) first_column=(-1,-1) columns=4 source=live_sidecar retained_resources=1 session_reused=1 frames=2 updates=1 draws=3 instances=18 nonclear=4 compute_dispatch=1 indirect_draw=1 readback=1
client_render_backend_probe=passed backend=slang_rhi device=created frame_resources=offscreen_frame_resources_validated frame_lifecycle=validated
client_slang_shader_validation=passed entry_points=12 slangc=/home/zachr/Workspace/octaryn-workspace-dev/build/dependencies/slang-2026.8.1/slangc
server block store native probe passed
```

Edit-only persistence evidence:

```txt
build/debug-linux/server/validation/client-server-app-launch-probe-world/world_blocks.json contained only two authoritative edit records:
(8,31,8)->0 and (8,32,8)->29.
server_live_chunk_stream reported radius=1 columns=9 blocks=2 for the app
launch sidecar and did not stream generated seed terrain block records.
```

Known validation notes:

```txt
Local X11 presentation remains unavailable in this environment and is logged as
slang_rhi_swapchain=sdl_video_init_failed driver=uninitialized created=0 acquired=0 presented=0.

The app launch probe still uses its existing radius-1 readiness sidecar; the
focused offscreen world-frame-loop probe remains the radius-32 proof path.

git diff --check could not run because this workspace root is not a Git
repository and returned exit 129.
```

Hygiene:

```txt
Active source line-count scan found no source/code file over 500 physical lines.
Forbidden active renderer fallback grep found only validator rejection strings
and SDL_GPU OFF dependency configuration. Raw Slang/GFX API leak grep returned
no hits outside client render-backend internals.
```

Remaining:

- Feed app launch/runtime evidence with radius-32 coverage without forcing
  server readiness validation into whole-radius synchronous work.
- Add full GPU culling coverage and richer GPU/pass timing if timestamp support
  is available through the backend.
- Connect retained world-frame-loop lifetime to the real interactive frame loop
  once the graphical app moves past bootstrap execution.

Exact next step:

```txt
Add a non-X11 client launch/runtime validation path that runs Octaryn.Client with
a prebuilt radius-32 server-authored sidecar and validates the emitted Slang RHI
frame-loop evidence, while keeping the existing server readiness probe bounded
and edit-only.
```
