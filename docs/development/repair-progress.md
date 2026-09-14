# Repair progress — 2026-09-13

Active root: C:\Users\Rose-X\Documents\Octaryn
Backup remains untouched. Read feature-parity.md and slang-vulkan.md for detailed
source mapping and narrowly scoped evidence. Historical restoration.md records
what was true before these repairs, not current build completion.

Latest visual follow-up: [cloud/water jitter and blue colors](cloud-water-jitter.md).
It corrects the FSR projection-jitter convention, adds real forward-layer
multi-frame evidence and implements the requested vibrant sky/deep lake water.

The FSR and moving-delivery follow-up is recorded in
[fsr-streaming.md](fsr-streaming.md). It corrects opaque object reactivity and
replaces normal column delivery's blocking GPU waits with two bounded jobs.
The report separates measured movement, load throughput, visual evidence and
backend qualification from the historical stationary results below.

The subsequent [exact cave-cache pass](terrain-streaming-cache.md) reduced
radius-32 full-column population from about 89 seconds to 45 seconds on DX12 and
50 seconds on Vulkan, preserving every voxel and the measured geometry. CPU,
stream/server lifecycle and packaged native-render checks pass. Synchronous GPU
mesh waits and moving-center streaming remain separate work.

The current UI/items/FSR pass is recorded in
[presentation-integration.md](presentation-integration.md), with per-platform
proof and the boundaries of the counted creative inventory. Its DX12 batching
capability fix supersedes the per-column-only limitation in the earlier DX12
switch report. Historical test counts below remain historical evidence.

Latest evidence is in the final "Active Slang RHI presentation verified" and
"Presentation corrections and safe packaging" sections below, and in
pipeline-parity.md. The initial bootstrap findings below are historical.

## Current pass

The Windows x64 release aggregate build PASSES using tools/build/windows.ps1,
clang-cl, the Visual Studio developer environment, local CMake/Ninja, .NET SDK,
and Slang 2026.17.1. The packaged server activates the actual basegame and native
world/player systems, reports ready, shuts down, and exits 0. The packaged client
runs its existing three-frame diagnostic fixture and Windows swapchain from
C:/Windows with confirmed Khronos Core + Synchronization validation: zero errors,
zero warnings, exit 0. This is a diagnostic baseline, not playable game completion
or desktop platform parity.

Completed changes so far:
- Native Windows compiler/tool entrypoint, architecture-separated output trees,
  and a run-client action that launches the .exe from its bundle directory.
- .NET installation/hosting discovery, nethost-based Unicode hostfxr loading,
  required DLL staging, and explicit failure for unavailable Windows hosting.
- Slang SDK discovery, explicit Vulkan/direct-SPIR-V device configuration,
  executable-relative shader lookup, complete bundle shader failure semantics,
  and one shared compute-pipeline compiler.
- Python interpreter discovery for CMake; native Windows Python command and
  safe quoted directory arguments in MSBuild; SDK-relative Roslyn references.
- Windows DbgHelp symbolizer for cpptrace, consistent dynamic CRT for Jolt,
  and compiler-front-end-correct warning switches.
- Correct preservation of an explicitly empty Git submodule list in dependency
  fetching. Unneeded submodules must not be fetched through argument loss.
- Repository NuGet.Config isolates restores from unrelated user feeds (a missing
  Stride development feed was breaking bundle publication).
- Client block snapshot FIFO replaces silent no-op apply/drain exports, with
  validation/backpressure and lifecycle reset.
- Terrain cleanup and player block lookup use explicit C ABI calls across DLLs,
  keeping internal C++/STL implementation details inside the blockstore owner.
- Portable environment setup and explicit fixture setup failures for the
  existing voxel raster/frame-loop probes.

Verified independently:
- 26 Slang modules and 12 entry points compile; real production Vulkan occupancy
  dispatch/readback returns expected counts, including packaged execution outside
  the repository and failure when a required packaged shader is absent.
- Windows hostfxr discovery/link/staging/required exports load in a focused native
  executable; missing hosting fails configuration.
- Managed snapshot ABI regression checks pass; managed owner build passes with
  zero warnings/errors in the isolated feature-parity-windows preset.
- Native ABI and CMake policy-separation validators pass.
- Basegame worldgen and catalog/atlas/animation content validators pass.

## Run and continue

From the active root:

```powershell
.\tools\build\windows.ps1 -Action configure
.\tools\build\windows.ps1 -Action build
.\tools\build\windows.ps1 -Action run-client
```

Tools are under build/dependencies/tools. The configure may fetch dependencies.
The restored client currently remains a finite diagnostic bootstrap, so launching
it is not equivalent to interactive gameplay. Windows swapchain integration, relocatable bridge defaults, and internal probe linkage now pass their focused checks.

After aggregate build and actual native runtime checks, recover the continuous
client loop, input/camera/player snapshot consumer, server supervision, and
renderer application of authoritative edits. Actual transport integration then
needs source-time interpolation and the preserved stress-work invariants.
No 1:1 feature parity, network end-to-end, Linux/macOS runtime support, or full
Slang RHI replacement has been proven. Do not broaden into lighting redesign.

A thread follow-up loop is enabled every 30 minutes to continue focused repairs,
record verification and the next blocker, and notify only for meaningful changes.
## Completed baseline verification and next pass

- Full native/managed/bundle aggregate: build/build-windows.log, exit 0.
- Actual dedicated server startup with basegame validation, native terrain and
  Jolt-backed player owner: logs/server/repair-baseline-console.log, exit 0.
- Jobs and authority tick native checks passed. World time, block store, Jolt
  player simulation, terrain generation, and full persistence native checks
  passed; logs/server/native-checks records their evidence. The original
  persistence failure was Windows separator-sensitive test comparison; actual
  filenames/components and error-code assertions remain strict.
- Both real owner bundles pass native hostfxr export/invalid-input validation:
  logs/client/hostfxr-bridge-exports.log and server equivalent. Relocated Unicode
  managed activation also passed in focused fixtures.
- Nine actual GPU/resource/swapchain probes pass with confirmed Core+Sync layers,
  no unavailable stubs and zero errors/warnings:
  logs/client/gpu-probes/fixed-results.json.
- Actual packaged client from C:/Windows passes Core+Sync validation with matching
  source/bundled Slang shader hashes:
  logs/client/packaged-validation-fixed-results.json. Three frames, three draws,
  36 instances, 24 nonclear pixels. Four retained chunks, not full-radius terrain.
- Full shader compilation remains 26 modules + 12 entry points; see slang-vulkan.md.
- Final CMake policy, native ownership/ABI, and client/server bundle ownership
  checks passed. All 69 touched code/script/CMake files checked were <=500 lines
  at the recorded check; subsequent edits must retain that limit.

Additional defects repaired after initial build:
- GFX ShaderResource barriers only synchronized fragment consumers. General
  memory barriers now cover compute/vertex consumers; expected counts unchanged.
- Swapchain acquisition is ordered before initial image transition without an
  added CPU wait or extra submission.
- Raw Vulkan vertex/instance shader semantics remove unrequested DrawParameters
  and correctly retain indirect firstInstance offsets into packed geometry.
- Raster color targets transition to CopySource for readback and back for reuse.
- zlib previously linked shared and static variants together, producing a missing
  z.dll dependency. The wrapper now chooses one static variant first.
- Internal blockstore/world-time probe code reuses object implementations without
  exporting STL internals; required world-time C ABI symbols are explicitly exported.

The radius-32 metadata check also runs successfully, but advertises 4225 columns
while retaining only 4 chunks. Renamed radius32_visible to
radius32_stream_available so it cannot be mistaken for full geometry coverage.

The terrain ingestion slice is now implemented and independently checked:
shared basegame generator extraction, strict binary server snapshot parsing,
full-depth reconstruction with authoritative overrides, and a bounded worker
queue. Direct native validation passed 6,534 comparisons with the pre-extraction
server DLL plus edited-air, malformed-file and asynchronous-window cases. See
feature-parity.md for scope and the standalone world-stream probe. Interactive
renderer and local-session integration are still being qualified separately.

Interactive Windows integration is now running:
- Default Octaryn.Client launches a continuous SDL window and supervised server;
  `--diagnostic` retains the old three-frame diagnostic (verified exit 0).
- WASD/mouse controls, server Jolt player poses, source-time history, graceful
  shutdown/save, bounded asynchronous terrain generation and GPU residency are
  connected. The camera uses the existing owner's 90-degree vertical FOV.
- The actual renderer capture in logs/client/open-world-fixed.bmp shows terrain
  and sky. It is the same retained color image copied into the swapchain.
  logs/client/open-world-fixed-validation.log records 300 rendered terrain frames,
  25 full-depth columns, 1,039,331 GPU-generated faces, 856,810 nonclear pixels,
  confirmed Core+Synchronization validation, and zero errors/warnings.
- Real black-screen defect: GFX retained a pointer to an expired local depth-clear
  value. Clear values now live with renderer resources. GFX already flips viewport
  Y; removing a second shader flip restored image orientation.
- A longer run found fatal transient player-snapshot replacement contention.
  The live server now skips that publication and retries the latest state on the
  next normal tick, retaining a coherent snapshot. A forced two-second lock
  reproduced the error and recovered during a 32-second authoritative session.
- Actual session movement, source clock, graceful stop, and saved-position reload
  passed: build/release-windows/tools/local-session-check/check.log and
  session-check-logs/long-run.log. Pose history assertions cover duplicate/stale
  rejection, 1x playback, hold/refill, teleport and contact behavior.
- Input sends preserve fractional time remainder; resetting it had halved the
  send rate near 61 FPS. Binary snapshot readers now allow Windows replacement;
  the rebuilt world-stream probe passed 6,534 samples and sharing/parser checks.
- Normal client was launched through CLI and left running (PID 12944 at this
  verification). logs/client/open-world-live.bmp independently shows terrain/sky.
  No app-control or UI automation is allowed in subsequent work.

Restoration direction clarified after recovering the omitted original reference:
read presentation-restoration.md before further presentation work. The next step
is original atlas/material/animation integration, followed by the original scene
pass flow and implemented lighting/UI behavior. The minimal WorldRenderer is
temporary bring-up scaffolding, not the final feature pipeline.

Remaining integration sequence (subordinate to that source-based recovery map):
1. Connect live block interaction and block-edit queue consumption to the world.
   Keep validation and persistence in the server.
2. Restore atlas/material/decoration presentation, transparent water, and runtime
   menus. Current rendering uses plain material colors and opaque water.
3. Add neighbor-aware face culling and greedy merging; qualify larger radii and
   moving-camera coverage. The current bound is 25 columns, not radius 32.
4. Integrate actual transport using the preserved networking timing/queue/lifecycle
   invariants, then measure corrections, snapshot age and frame pacing under load.
5. Qualify desktop platforms individually and close remaining feature-parity rows.

Profiling follow-up: allow shared CSV readers (current secure CRT writer locks the
file while running), and populate client input camera XYZ for meaningful legacy
server camera-error diagnostics; zero XYZ currently makes those diagnostic errors
misleading while actual authoritative presentation uses the received player pose.

Do not rebuild already verified systems repeatedly without new changes or a
reproducible failure. Continue from this baseline and record each next integration
with evidence. No source was removed from the preserved backup, and no Git commit
or push was made.

## Original presentation integration and performance checkpoint

The later checkpoint supersedes the plain-color/radius-2/UI-disconnected state
above. See `presentation-restoration.md` for exact restored behavior and remaining
differences. The current backend remains **Slang GFX + Vulkan**, not standalone
slang-rhi or implemented Vulkan ray tracing.

Native aggregate and bundle builds pass. First-party shaders compile as Slang
translation units and 18 entry points; include-only fragments are validated
through their parents with complete source coverage. Native owner boundary
validation passes, and active source/code files remain at most 500 lines.

Runtime validation now includes textures/mips/animation, sky/day-night, the original
menu/HUD, torch/flower geometry, glass and separately classified fluids. Corrected
sky-resource destruction is covered by clean Core+Synchronization teardown in
`logs/client/presentation-fixture-validation.log` and
`logs/client/presentation-menu-validation.log`. GPU readback contains 3,353,359
valid faces across 81 columns in the isolated fixture.

First-person block commands are routed through existing server validation and
persistence. The isolated real-session harness proves break, forged-camera/reach
rejection, recovery after rejection, placement/removal and restart with exactly
two persisted air overrides. No local predicted terrain mutation was introduced.
World time travels with coherent player snapshots, including midnight interpolation.

Performance changes reuse Camera visibility and cache the framebuffer until resize.
Local session pose I/O is paced at 60 Hz while presentation still advances every
frame; unchanged payload parsing and idle command-file polling are avoided. The
controlled session probe reduced pose reads 2,400 to 600 and mean update cost
174.217 to 144.751 microseconds. This isolated result is not a live FPS claim.

`--benchmark-seconds 15` uses a fixed initial view, 1280x720, radius 4, no gameplay
input and five seconds of warmup after all columns arrive. CSV readers may now
read the live file. Final logs include the eight slowest measured frames with
session, streaming/meshing, rendering, event and profiling-tail attribution.
Do not report average FPS alone: one run reached 1.652 ms average / 333 FPS
histogram 1% low, but later runs exposed 0.555–1.737 second pauses outside measured
session/mesh/render work. The detailed attribution run must be inspected before
claiming these stalls resolved or making a clean comparative speedup claim.
The detailed run (`presentation-hitch-attribution.log`) isolated its worst
63.885 ms frame to 61.025 ms inside LocalSession, with only 2.602 ms rendering.
The earlier unexplained event/tail pauses did not recur in that run. The bounded
`App/LocalSession/SessionIo` worker now owns pose/input/window/ordered-edit I/O.
Main-thread mailboxes never hold their lock during disk operations; unsent input
is latest-only and expires rather than being resent as a stale heartbeat.
The injected blocked-read probe proved zero frame-thread file reads/writes,
latest-only delivery, stale expiry, source-time behavior and clean join. The real
edit/rejection/placement/restart harness passed again with the final worker.

Final live evidence: `logs/client/presentation-async-io-benchmark.log` identifies
the active AMD Radeon RX 9070 XT through GFX, and records 1.593 ms average
(about 628 FPS), 400 FPS histogram 1% low, and 40.960 ms worst over 15 measured
seconds after warmup. Sampled session cost is about 0.001 ms; all four logged
slow frames belong to rendering, not file I/O. This verifies removal of the
observed frame-thread filesystem stall, not elimination of every hitch. The
remaining whole-queue waits, allocations/ordering and moving-world load require
further measured work. These are fixed-view Windows results, not cross-platform
or general gameplay performance guarantees.

Full G-buffer/HDR, original fluid shading/flow, clouds, remaining lighting/player
presentation and platform/transport work remain open. In particular, the gray
fixture water is a visible missing original forward shader, not completed parity.

The final worker build also passed Core/Synchronization validation through
shutdown (`presentation-final-validation.log`, 1,381 frames, zero errors/warnings).
Normal `saves/open-world` was then launched via CLI without validation overhead,
working directory `C:\Windows`, PID 14684 at verification. Its log identifies
Slang GFX/Vulkan/RX 9070 XT, restored saved eye position, and 81 resident columns.
`logs/client/open-world-restored.bmp` is the actual copied-to-swapchain image.
No app control or UI input injection was used. The process was responding and
left running; that PID is launch evidence, not a perpetual availability claim.

## Standalone slang-rhi cutover verified

The user's immediate backend correction is implemented. The interactive world
now uses upstream standalone slang-rhi (e17f6d75), statically linked with its
Vulkan backend and Slang 2026.17.1 shaders. GFX has no active library/DLL linkage.
See [slang-rhi-migration.md](slang-rhi-migration.md) for source, build, GPU capture,
linkage and complete runtime evidence, remaining retired probes, and performance
limits. Final packaged Core/Synchronization/RHI diagnostic passed 180 fully
resident frames, 81 columns, 3,353,335 faces, zero warnings/errors through shutdown.
Fixture GPU readback passed all 3,353,359 records and five expected material cases.

The original session/interpolation/world-stream fixes are preserved. Two local
1280x720 fixed-view benchmarks measured 1.276/1.090 ms average, but first-run
render stalls reached 224 ms; do not claim all frame-pacing issues fixed. Resume
HDR/fluid/lighting/player restoration on actual RHI. No GFX-specific feature
restoration, source aliases, or demonstration renderer is the active direction.

Normal saves/open-world was launched from the final bundle via CLI, working
directory C:/Windows. PID 14920 was responding with 81 resident columns at
verification; logs/client/open-world-slang-rhi.log records the actual RHI/Vulkan
identity and restored authoritative pose. The GPU capture
logs/client/open-world-slang-rhi.bmp was visually checked. No app control used.

## Active Slang RHI presentation verified

The sky, four-target G-buffer, HDR resolve, forward fluids, player skinning,
clouds, selection, tone mapping and RmlUi now form one active standalone RHI
pipeline with Slang shaders. Original reference mapping and remaining limits
are recorded in pipeline-parity.md. The configured source/include/shader audit
passes 73 sources, 65 headers and 24 active shader modules. All 49 shader files,
30 complete modules and 28 entry points compile; bundled sources match exactly.

Runtime checks caught and repaired the wrong packaged player asset path and
first-person torso self-occlusion. The native player probe passes eight authored
clips, transition/interruption checks and the 144-index first-person limb filter.
The full 216-index third-person mesh is visibly rendered. Its white materials
are the retained asset's actual untextured appearance.

Final strict Vulkan Core/Synchronization/RHI runs completed with 81 columns and
zero GPU warnings/errors through shutdown: pipeline-first-person-final.log
(3,113 frames) and pipeline-fluid-levels-final.log (3,214 frames). The fluid
oracle passes all 18 cases, eight levels and 60 faces with maximum error
5.96e-8. The authored geometry fixture passes all 17 expected faces across
torch, flower, glass, water and lava. With ambient zero, Sun strength zero to
three increases mean display luminance over 58,500 terrain pixels from 0.11248
to 0.45287. These checks do not establish shadows, GI or ray tracing.

The fixed 81-column Windows RX 9070 XT benchmark at 1280x720, without validation
or capture, measured 17,114 frames after five seconds of warmup over 15 seconds:
0.876 ms mean, 666.67 FPS histogram 1% low, 2.335 ms worst. This is not a
terrain-version-matched comparison or moving-world benchmark. Nine LocalSession
files remain byte-identical to the pre-RHI snapshot; separate terrain-protocol
changes are not incorrectly included in that preservation claim.

The separate terrain revision 2 update rejects unversioned edit-only saves.
Existing saves/open-world remains preserved. tools/run-client.ps1 launches
saves/open-world-v2 by default and accepts -WorldDirectory explicitly. A normal
new revision 2 world was launched via CLI with --third-person from C:/Windows;
PID 13560 was responsive with 81 resident columns when checked. Its actual GPU
capture is logs/client/open-world-pipeline-final.bmp. F4 returns to first person.
No app control or synthetic input was used.

## Presentation corrections and safe packaging

The next repair pass removes flight-descent-driven false crouch, restores
original sprite normal/specular LOD0 sampling, and bounds RmlUi retention logs.
The native client/player probe and full client/server bundle build pass. All
49 Slang sources, 30 modules and 28 entries compile; the updated build-graph
audit passes 74 sources, 65 headers and 24 active shader modules. Changed sprite
stages also pass SPIR-V validation. Source and packaged shaders match.

The old bundle pre-clean partially removed unlocked files before failing on
loaded DLLs. Missing files were recovered from current build outputs. Packaging
now stages the complete client/server payload before installing it, retains old
bundles and rolls back failed replacement. Ten isolated filesystem tests pass,
including a real Windows sharing lock. The final bundle build reports identical
canonical/staged contents, verifying the recovery. See pipeline-parity.md for
logs and recovery details. PID 13560 stayed responsive; no app control, restart
or competing GPU test was used. The new executable applies on the next launch.

## Coherent presentation and one draw preparation per frame

PoseHistory now samples velocity at the presentation cursor and defers discrete
tick/contact/flight changes until their actual snapshot boundary. Existing
camera position interpolation and hold/refill behavior remain intact. The
production player probe passes the new source-time metadata regressions.
WorldRenderer now shares one retained visibility/uniform preparation between
opaque and forward passes; its canonical CPU probe passes all 116 checks,
including ordering, culling, storage reuse and column mutation. Native default
launches now use the same saves/open-world-v2 as tools/run-client.ps1.

Native checks pass in build/presentation-timing-build.log. Windows safely denied
the first bundle directory replacement while the client was running. Only the
executable differed, so it was backed up and atomically replaced. The final full
publish passes with identical staged/canonical contents in
build/presentation-timing-bundle-final.log. Backend audit, shader bundle equality
and owner boundaries pass; all 711 scanned code files satisfy the line limit.
See pipeline-parity.md and networking-recovery.md for provenance and limits.
The existing game stayed responsive; no new GPU capture, restart or FPS claim
was made. These corrections apply on the next launch.

## Rapid boundary reversal and original mouse sensitivity

WorldStream now invalidates completion metadata for every requested center,
including temporary centers missed by a busy worker. This allows GPU-retired
edge columns to return with an unchanged revision after rapid boundary reversal.
Large payload cleanup stays on the worker. The actual shared residency-owner
regression passes and fails against the old retirement policy. The real async
stream probe passes, including 24,480 terrain-parity samples. Mouse sensitivity
is restored to the original 0.1 degree per relative count.

Native checks and final complete bundle publication pass in
build/stream-residency-build.log and build/stream-residency-bundle-final.log.
The previous executable/hash manifest are backed up; the existing game remains
responsive and changes apply on next launch. Backend/source/bundle and owner
checks pass. No new GPU capture or moving-world performance claim was made.
See networking-recovery.md and pipeline-parity.md for exact evidence and limits.
Original action audio remains missing; action-audio.md records its implementation
source and event semantics for the next restoration slice.

## Original action feedback restored; complete package staged

The next slice restores all four original action sounds through a focused native
OpenAL/miniaudio owner and a declared basegame catalog. Existing interaction
edges play once when selection succeeds or an edit enters the local queue;
server authority is unchanged. The owner handles unavailable devices and
bounded eight-voice saturation. Native build, player/action-hook checks, 42 CPU
checks and 49 actual OpenAL loopback checks pass in
build/action-audio-reviewed-build.log. Speaker output and actual packaged
gameplay audio remain unverified; see action-audio.md for precise coverage.

The complete 151-file staging payload passes shader equality, client/server
module policy, dedicated/bundled server equality, compiled manifest comparison,
audio/catalog/license hashes and owner boundaries. All 723 scanned code files
remain within 500 lines. Active render-graph policy passes 78 sources, 69 headers
and 24 shader modules. Evidence: logs/client/action-audio-stage.json and
action-audio-render-audit.json. No first-party shader or GPU backend changed.

Canonical installation is pending. build/action-audio-bundle-final.log records
Windows denying replacement of the bundle held by responsive PID 13560. The
seven changed and five added payloads remain coherently staged; no live DLLs,
save files or partial client package were replaced. Rebuild the bundle after
the game exits. This is successful compilation and staging, not installed or
runtime-qualified audio.

A subsequent audit corrected the initial removed-block-mask gap classification.
The retained helper is unwired, but current delivered columns already build and
swap their GPU mesh synchronously before the next draw. Adding a mask at that
point has no visible benefit. The original behavior hid changed non-air blocks
in opaque/sprite passes while its asynchronous replacement uploaded; its helper's
absence alone does not prove a current rendering failure. Player skinning and
all active passes remain on standalone Slang RHI with Slang shaders.

## Camera and targeting follow delivered terrain

The deeper removed-block audit found a concrete CPU/render coherence issue:
worker-generated query data became visible before its matching column left the
ready queue. Camera collision and targeting could read not-yet-displayed edits;
rapid away/back requests could also expose GPU-retired query data. The fix pairs
each queued payload with its immutable query copy and publishes both at delivery,
followed by OpenWorld's existing synchronous GPU upload before queries/draw.
Visibility is invalidated immediately on window exit. Queues and worker-side
retirement stay bounded, with no additional block copy or shader change.

Native build and production player/residency tests pass. The exact production
owner regression fails when only the old early-publication policy is restored
in a scratch header. The actual threaded stream probe passes withheld-delivery,
air/re-add and reversal checks plus 24,480 terrain-parity samples. See
networking-recovery.md and logs/client/query-delivery-{green,red,world-probe}.log
for evidence and the required delivery/upload/query ordering. No new GPU capture
or frame-performance result is claimed.

Final native qualification passes in build/query-delivery-reviewed-build.log,
including the new canonical octaryn_validate_client_world_stream target. That
target supplies target-derived Windows runtime search paths; directly launching
the bare probe without them initially failed with missing DLL status 0xC0000135.
The full 151-file staging payload passes in logs/client/query-delivery-stage.json,
including source shader equality, module/server contents, hashes, owner rules
and the 723-file line-limit scan. Active RHI graph audit passes 78 sources,
69 headers and 24 Slang modules in query-delivery-render-audit.json.

build/query-delivery-bundle.log again reaches complete staging and safely fails
canonical installation on the existing game/session file lock. Both this query
repair and the preceding audio restoration are staged together, not partially
installed. The game remains responsive. After normal exit, run the full bundle
build before launching again; the current in-memory session retains its old code.

## Opaque pipeline state and walk/run transition stability

Restored original Back/CounterClockwise/LessEqual opaque state through the
WorldRasterPipeline owner. Sprites use a separate None/LessEqual pipeline;
forward blending/depth behavior remains unchanged. The actual Slang/RHI headless
draw fixture passes six exterior/interior cube faces, both sides of four sprite
faces and ten equal-depth redraws. Khronos Core/Synchronization and RHI validation
report no GPU errors/warnings. See logs/client/raster-culling-{gpu,validation}.log
and pipeline-parity.md. This is not a new full-world capture or FPS measurement.

The Run threshold moved from nominal walking speed 5 to a named midpoint 7
between walk 5 and sprint 9. This prevents float position-difference noise from
repeatedly switching Walk/Run and restarting transitions at constant walking
speed. Boundary and 64 quantized 30/60 Hz steps pass with the existing player
checks; all 116 draw-preparation checks also pass in build/raster-culling-build.log.

The complete staged package includes these fixes plus pending audio and terrain
query updates. Shader/module/server/hash and owner checks pass; 726 code files
satisfy the line limit and the active graph passes 79 sources, 70 headers and
24 Slang modules. The final bundle attempt stops safely at the running game's
directory lock (build/raster-culling-bundle.log). The responsive game and saves
remain intact; the new coherent package is staged, not installed.

## Avoid redundant neighbor rebuilds after edits

WorldMeshInvalidation now compares the actual facing boundary data before a
column replacement, preserving pending work and all diagonal fluid dependencies.
An interior edit schedules zero neighbor rebuilds, a noncorner edge one, and a
corner three instead of the previous unconditional eight. First load, unload,
invalid shape and vertical extent changes remain conservative.

The actual world_mesh_halo input oracle passes 3,264 checks over all 1,024
horizontal positions and vertical/residency/pending-work cases. Existing 116
draw-preparation and player checks pass in build/halo-invalidation-build.log.
No shader, GPU meshing algorithm or render-pass state changed. This proves reduced redundant
rebuild scheduling, not measured moving-world FPS improvement.

The full 151-file stage passes module/server/shader/hash and owner checks; all
728 code files satisfy the limit. The active graph passes 80 sources, 70 headers
and 24 Slang modules. Evidence: logs/client/halo-invalidation-stage.json and
halo-invalidation-render-audit.json. Installation again stops safely at the live
bundle directory lock in build/halo-invalidation-bundle.log. The responsive game
and saves are preserved; all pending fixes remain staged together.

## Preserve accepted block-action order

The client now retains a bounded 64-action sequence instead of merging each
frame's clicks and wheel movement. Place then wheel uses the block selected at
the click; wheel then place uses the updated selection. Repeated clicks remain
separate. Overflow preserves the first 64 actions with one warning per session;
earlier valid actions survive a later modal opening or focus loss. Server edit
checks and success-only feedback remain intact. See input-ordering.md.

The native client links and the canonical player-model CPU probe passes,
including action_feedback=passed, in build/ordered-actions-build.log. Coverage
includes actual queue/dispatch ordering, rejected edits and overflow prefix
preservation. Final stage/publication qualification remains pending; no injected
SDL events or new gameplay/GPU runtime validation is claimed.

Final ordered-action staging passes in logs/client/ordered-actions-stage.json:
151 files, coherent module/server/shader/native payloads, 730 code files within
the line limit, and the active graph at 80 sources, 71 headers, 24 Slang modules.
The canonical shader target freshly compiles all 49 sources, 30 modules and
28 entry/stage combinations (build/ordered-actions-shaders.log). Build and CPU
checks pass; canonical installation still stops safely at the live directory
lock in build/ordered-actions-bundle.log. The game and server remain responsive.

## Production player GPU visibility and animation

The new explicit octaryn_validate_client_player_rendering target uses the real
PlayerRenderer, retained glTF and Player.slang in a headless 128x128 RGBA16Float
and D32 fixture. Hidden output has zero covered pixels; the full model covers
1,350, and the limb-only index subset covers 804. Restoring full geometry at a
held source time is bit-identical. Walk samples change 375 HDR and six silhouette
pixels; attack samples change 1,352 HDR and 270 silhouette pixels. Held times
produce identical HDR and depth for both clips. Core/Synchronization and RHI
validation pass with zero GPU errors/warnings, including teardown, in
logs/client/player-rendering-validation.log. No SDL video, surface, window,
game input, live session or production save was used by this fixture.

The limb subset is framed from an external diagnostic camera; this is not a new
integrated first-person camera capture. Existing pipeline-*-final full-world
captures remain the combined presentation evidence. No new FPS result is claimed.
The local model is integrated; F4 exposes the full third-person mesh. Remote
avatars, shadows/GI/RT and full terrain PBR remain unfinished. The newly confirmed
authoritative fluid spreading/drainage gap is mapped separately in
fluid-simulation-recovery.md; the restored fluid renderer is not its simulation.

## Fluid evaluator and autonomous world publication

The original fluid evaluator and slope rules now compile in the native server
block owner, with a validated optional module contract and basegame catalog
provider. The canonical octaryn_validate_server_fluid target passes 424 checks,
including remapped IDs, original donor/source/contact/vegetation rules, signed
bounds and native apply/override/support/replication seams. Representative flat
donor read counts are 922 for water and 114 for lava; no runtime performance
claim follows from those counts. See logs/server/fluid-evaluator-validation.log.

The process stream now publishes actual authoritative changes independently of
new client commands. ModuleActivator advances a block revision only for changed
edits; per-instance ChunkPublicationTracker acknowledges it only after a full
successful snapshot write. Every process publication is full and self-contained:
the binary stream has no delta flag and the client replaces its override set.
Epoch-only refreshes now force native publication even at unchanged coordinates;
preserved columns retain their edits when windows move. Unchanged state stays
idle. The post-tick publication owner never executes another authority tick.
The normal LocalSession already requests full snapshots; generic native metadata
utilities remain unchanged.

The underlying managed server world-blocks probe passes, including 162 fluid
provider checks and real native JSON/binary publication, forced binary-output
failure/retry, autonomous same-window edits, no-op/rejected command revisions,
destination/instance isolation, epoch-only refreshes and exact retained overrides
in both JSON and binary after window movement. Evidence:
logs/server/fluid-publication-probe.log.
The canonical CMake aggregate built affected native/managed owners, then stopped
at the known client-bundle install lock before its probe command. Root ran the
underlying probe using that target's exact environment and native DLL paths;
do not report build/fluid-publication-managed.log as a passing aggregate.

The final bundle rebuild, build/fluid-recovery-bundle.log, compiled the reviewed
publication fixes and reached complete staging before the same WinError 5 install
lock. The refreshed complete 151-file stage passes in logs/client/fluid-recovery-stage.json.
It contains 18 changed and five added payloads relative to the retained canonical
bundle, with none removed. All 746 code files meet the line limit. The renderer
graph still passes 80 sources, 71 headers and 24 Slang modules; no shader or GPU
rendering changed in this slice. Game/server processes remain responsive; the
new package is staged, not installed, and no new GPU or FPS result is claimed.

Fluid ticking and native configuration marshaling remain inactive. Next: bounded pending work and replication
backpressure, successful-change notifications, simulation-time budgets, active
region/residency repair, then actual apply/persist/publish integration. The
unbounded BlockChangeQueue is not drained by the process-file client, so enabling
continuous fluid mutation now would introduce an unbounded backlog. Preserve
this prerequisite and the explicit incomplete status in fluid-simulation-recovery.md.

## Bounded replication and process snapshot ownership

The previous unbounded-replication prerequisite is resolved. Native
BlockChangeQueue is now a fixed 8192-entry FIFO. Edit application plans the exact
zero/one/two changes, including unsupported-above removal, and checks capacity
before mutating the store. A deferred client command stays at the queue front;
direct module application returns false without mutation. No-op and invalid
commands can finish even at capacity. Draining retains the existing
all-or-nothing contract; undersized buffers leave pending changes untouched.
The stable single-authority-thread contract remains required. No allocation
failure rollback or simultaneous independent replication consumers are claimed.

Standalone Host explicitly creates ProcessSnapshots authorities, including
moduleless and one-shot paths. They do not allocate a delta queue; changes still
update authoritative overrides, revisions and persistence. Full process output
remains retryable and retains distant edits for later windows. Default and
HostExports authorities keep ReplicationDeltas. Process-only delta drains return
unsupported instead of claiming an empty event history; regional output never
clears a global replication queue.

The canonical octaryn_validate_server_block_store_native_probe now actually runs
on native Windows/Linux and fails cross-compilation rather than silently skipping.
build/replication-backpressure-native.log passes the existing block-store probe,
32827 backpressure checks and 424 fluid-rule checks. The managed world-blocks
probe passes directly in logs/server/replication-backpressure-probe.log: actual
authority saturation, unchanged revision and persisted bytes while deferred,
complete FIFO recovery, exact once-only retries and on-disk reload. Process mode
applies 8194 module edits plus one queued client edit with zero delta backlog,
then passes native writer failure/retry, later distant-window output and reload.
This direct probe run uses the target's environment/native libraries; it is not
a passing CMake managed aggregate or active fluid scheduler claim.

The complete rebuilt stage passes logs/client/replication-backpressure-stage.json:
151 files, 18 changed and five added relative to canonical, none removed, all
750 code files within the line limit. The active rendering graph still passes
80 sources, 71 headers and 24 Slang modules. The final bundle replacement in
build/replication-backpressure-bundle.log encounters the unchanged WinError 5
live-file lock. Client 13560 and server 26080 remain responsive; installation and
new runtime/GPU/performance verification remain pending normal game exit.

Next fluid work: bounded pending positions with reliable retry/repair on
saturation, native rules configuration, simulation-time scheduling and active
region seeding. The existing evaluator has not been connected to live fluid
mutation; repeated fluid save costs and scheduler timing remain unqualified.

## Authoritative fluid scheduler integrated

FluidSimulation now marshals optional module fluid configuration into an owned
native service. FluidScheduler restores event/continuation delays, bounded
deterministic deadlines, active regions with a read-only sampling halo, retry
and cyclic repair. The original repair predicate avoids waking stable enclosed
sources; qualifying repair is immediate, so 250/500 ms delays are not absolute
minimums for repaired positions. Unknown data retries then returns to repair,
while replication backpressure retains pending proposals without mutation.

ModuleActivator receives complete changed-block lists from command owners.
They wake fluids and preserve changed-only revision/persistence behavior. It
services one native fluid step after authority/module work and before saving;
native primary/support changes use the existing apply policy and mark revision
and persistence once per changed fluid batch. Standalone process view intent
configures the region before ticking. No region means no simulation, including
default ABI hosts until a region owner is supplied. No duplicate authority tick
or rendering backend change was introduced.

build/fluid-scheduler-native.log passes 17099 fluid evaluator/scheduler/CABI checks
and the existing 32827 block backpressure checks. Cases include delay/order,
capacity and budget limits, unavailable/blocked retries, region move/repair,
stable pools, copied configuration lifetime, invalid time, and support-cascade
counts/replication. logs/server/fluid-scheduler-probe.log passes actual managed
authority water/lava falling into correct content IDs, native output without a
new client command, save reload and the existing publication/backpressure suite.

The managed probe uses actual basegame registration/generated terrain over
81 active columns for 150 ticks (30 warmup, 120 measured, console suppressed).
Fluid step mean/p95: 1.931/2.005 ms. Whole authority tick mean/p95/max:
2.253/5.414/5.952 ms. Nine fluid changes, maximum seven pending positions and
4180 reads, 81 budget stops. The whole tick includes player/authority/save work;
this is a headless server workload, not client FPS or an extensive fluid benchmark.
FluidReport/FluidStepMilliseconds and changed-fluid log entries expose profiling.

Full bundle rebuild reached complete staging, then hit the same live directory
lock in build/fluid-scheduler-bundle.log. logs/client/fluid-scheduler-stage.json
passes all 151 files, 18 changed/five added/no removals versus canonical; 762
source files respect the line limit. The renderer audit remains 80 sources,
71 headers and 24 Slang modules. Client 13560 and server 26080 remain responsive;
latest fluid simulation is not installed in those processes. No new GPU capture.

Next qualification: large/moving-region repair latency, sustained fluid save and
sampling cost, contact/spreading/drainage in combined live presentation after
installation. The original full greedy/prefix/visibility rendering path, remote
avatars and other desktop platforms still need work. Keep advanced shadows/GI/RT
and terrain PBR separate from claims about originally working ambient/emission
lighting. The repair loop remains active; this is not full engine parity.

## Terrain sampling performance

Native terrain reads now reuse a bounded thread-local 256-entry geometry cache.
Full signed coordinate keys guard collisions; material rules, cave Y sampling and
live overrides remain outside the cache. The compiled generator seed/revision
and saved baseline do not change. Snapshot count also uses existing block counts
instead of allocating/sorting a full snapshot; save cadence and failure semantics
are unchanged.

build/terrain-cache-native.log passes 29912 scalar parity samples, 2048 concurrent
samples, 24480 client WorldStream comparisons, snapshot count/fill cases, 32827
backpressure checks and 17101 fluid checks. Fixed-work benchmarks with equal
checksums reduce vertical scans from 11.457775 to 1.637575 ms and neighborhood
reads from 7.864500 to 4.040800 ms. Deliberate misses at 12.156850 versus
12.206550 ms do not benefit.
See terrain-sampling.md for workloads and limitations.

logs/server/terrain-cache-probe.log passes the actual generated-world fluid,
publication and persistence fixture. Fluid mean/p95: 0.688/1.175 ms; whole authority
mean/p95/max: 1.015/4.036/5.000 ms (120 measured after 30 warmup). Nine changes,
maximum seven pending positions/4180 reads, zero budget stops. These are server CPU measurements,
not GPU or FPS results, and a time-budgeted scan can now complete more work.

The refreshed complete 151-file stage passes logs/client/terrain-cache-stage.json:
18 changed/five added/no removals; 765 code files meet the line limit, with the
same 80-source/71-header/24-Slang-module active rendering audit. Bundle installation
in build/terrain-cache-bundle.log still hits the running-game WinError 5 lock.
Client 13560/server 26080 remain responsive and retain their previous code.

Next restoration gap confirmed: natural vegetation is not called by the active
server/client terrain generator despite existing managed emitter rules and tests.
vegetation-recovery.md maps original noise/emission and the scalar/bulk/override
contract to restore. This is separate from current terrain reconstruction parity
and must respect existing generation identity and saves.

## Player movement and F3 priority repair

User-reported movement and F3 lag took priority over vegetation. See
movement-and-debug-performance.md for causes, measurements and limits.
Publication preserves its 60 Hz phase; successful host-only ticks advance the
same source clock. PoseHistory distinguishes a valid endpoint from an outage.
Host and client I/O use owned high-resolution Windows waits with absolute
deadlines and skipped missed slots. Input expiry, authority and queue bounds
remain intact. Native probes measure host median 16.034 ms and I/O 16.613 ms.

Jolt's separate rounded voxel bodies caused periodic sprint slowdowns. Sharp
cubes in one static compound preserve voxel geometry and enable internal-edge
removal. All 64 directional/cadence cases and existing collision tests pass;
ten-second sprint is 90.0006 blocks instead of 88.8441. Actual LocalSession and
packaged-authority movement reports zero holds/underruns and exact 1x playback.
This is local file transport, not remote-network qualification.

F3 retains rows/labels and changes numeric text. Immutable RHI Upload geometry
avoids per-buffer transfer submissions. Gameplay input skips menu synchronization;
hidden panels skip per-frame formatting. Final F3 profiling reports mean 0.804 ms,
1% low 571.43 FPS and worst 11.122 ms, versus original mean 1.396 ms, 1% low
35.71 FPS and worst 59.916 ms. Maximum UI update is 0.508 ms including startup.
Intermediate outliers are retained; occasional render-stage stalls remain under
investigation. Optional UI stage profiling reports its eight slowest updates.

The previous user processes exited normally. The coherent package now installs
successfully (build/movement-ui-verified-bundle.log). Installed qualification
passes 151 files, 779 code files within 500 lines, Slang/RHI ownership and payload
equality (logs/client/movement-verified-stage.json). Final packaged GPU validation
passes 550 UI checks and 4,444 resident frames, including player/F3 capture, with
zero RHI/Core/Synchronization warnings or errors. Direct gameplay event tests
inject no OS input. Managed publication, replication, fluid and save tests pass.
Tests use isolated worlds; production saves and backups remain untouched.

Next priorities: remaining render-stage spike attribution and short discrete-input
delivery/acknowledgement, then safe vegetation revision routing and broader parity.
The pure vegetation helper is preserved and passes 3,167,470 checks, but is not
wired to generation. The repair heartbeat remains active; full engine parity,
remote networking and other desktop platforms are not claimed complete.

## Arms, shoulder cameras and restored distance choices - 2026-09-13

First person now submits only the original arm triangles. Third person defaults
right; V swaps left/right and F4 toggles first/third, with no centered shoulder
mode. Camera collision covers the combined backward/lateral boom. Picking follows
the render camera while reach and edit origin remain at the authoritative eye.

The seven original selector choices (4, 8, 12, 16, 20, 24, 32) are restored and
Apply updates the live session, stream and renderer. Shared lossless terrain pages
remove the duplicate dense payload; center-first scheduling avoids full-window
rescans for every generated column. Existing two-entry delivery and exact query
pairing remain tested. Future 128-distance plans are not claimed implemented.

Radius-32 qualification exposed an AMD driver hang in vkAllocateDescriptorSets.
The pinned SlangRHI bootstrap now applies one exact checked-in descriptor capacity
patch, rejecting unrelated changes. Actual allocator mock checks and production
4,096-player-per-frame GPU tests pass pool growth/reset without diagnostics.
The repaired world loads all 4,225 columns and exits normally. The short stationary
maximum-distance sample averages 22.896 ms (~44 FPS); load-time spikes and playback
underruns remain performance work, not hidden behind the old radius-4 cap.

Live Apply passes 4 -> 8 -> 4 with actual 81 -> 289 -> 81 renderer columns. Camera,
arm filtering, collision, picking, terrain parity and 573 UI checks pass. The
coherent bundle installs and remains entirely Slang/standalone SlangRHI. See
camera-and-render-distance.md for source mapping, exact evidence and limitations.

## Full-detail voxel performance - 2026-09-13

No LOD is permitted by the current user request. Production Slang GPU meshing now
uses bounded bitmask greedy rectangles for occluding opaque cubes, retaining full
material/direction keys, tiled textures and unit geometry for cutouts/specials.
Opaque/sprite order is restored near-to-far; transparent order is preserved.
Per-pass SlangRHI roots/atlas bindings are reused, renderer statistics are cached,
and unchanged center/radius updates no longer scan the retained world.

Device timestamps and CPU stages are opt-in. Benchmark warmup waits for neighbor
remeshing to drain. On Windows/RX 9070 XT, distance 4 rises from 1,119 to 1,651 FPS
in stationary samples. Distance 32's final sample is 139.69 FPS with 15,751,869
quads and 311,815,488 bytes tracked mesh/frame GPU storage, versus 59,854,304 quads
and 1,017,454,448 bytes before. Its 80.48 FPS long-baseline window is provisional
because that process lacks a normal completion record; a later incomplete repeat
is excluded. The final package exits normally. See voxel-performance.md for method.

CPU 3,264 halo/137 draw checks, 14 exact GPU surface fixtures, 22 raster comparisons
and two byte-identical multiple-column binding tests pass. Cutout pixels are
identical; other raster differences require measured sparse sampling/subpixel
boundaries. Packaged core/synchronization validation passes 573 UI checks per
world, normal and fluid captures, and live 4 -> 8 -> 4 Apply. Fluid capture covers
60 faces, all eight levels and 18 cases within 5.96e-8. Two existing unused-output
shader warnings remain per packaged validation run; no validation errors. All 152
payload files and 800 code files pass the installed/owner/render-graph/500-line
checks. Backups and production saves are intact.

Streaming is unfinished performance work: the final maximum-distance run still
has a 2.359-second load-time outlier and two movement-buffer underruns. Command
encoding averages 4.71 ms versus 1.69 ms GPU after settling. Next prioritize shared
buffer/indirect submission batching and bounded asynchronous mesh count/readback/
emit, preserving revision/halo correctness. This does not complete remote networking,
other desktop qualification or the broader engine feature-parity repair loop.

## Voxel correctness follow-up - 2026-09-13

Fixed three independently reproduced defects without LOD or backend changes.
Existing-source boundary edits now promote already-pending neighbors ahead of
initial loading work; ordinary halo rebuilds choose the nearest resident column.
The queue still performs one rebuild per rendered frame. The apparent live queue
plateau was the minimized-window pause, not lost invalidation.

Camera frustum extraction now uses the projection's zero-to-one clip depth. The
old extraction failed to reject boxes beyond the far plane. An independent
projected-box oracle passes 12,006 perspective/orthographic checks after the fix;
the old code fails the explicit far-plane regression. This was overdraw, not a
proven explanation for nearby missing faces.

WorldRaster restores original chunk-relative arithmetic before adding fractional
torch/fluid geometry. A GPU regression fails with the old expression in an isolated
staged shader and passes 3,456 scalar checks at signed offsets of 16,777,216 with
the new expression. This protects distant geometry; it does not expand world or
physics coordinate support claims.

Actual GPU culling on/off produces byte-identical four-target/depth images in
33 views over 25 retained signed columns, with pitch, yaw, aspect and zoom varied.
The existing exact surface, cutout, material and multiple-column binding tests
also pass. CPU halo/scheduling checks total 3,343 and draw checks 137.
Evidence: build/voxel-repair-final-build.log and build/voxel-relative-before-gpu.log.

The installed package passes normal terrain and core/synchronization-validated
fluid runs, each with 81 columns and normal exit. Terrain retains all 1,483,600
unit surfaces in 320,431 rectangles. Fluids cover 60 faces, all eight levels and
18 fixture cases within 5.96e-8. The validated run has two existing unused-output
warnings and zero validation errors. Saves/backups are preserved. Remaining
streaming GPU waits and broad engine/platform parity are not complete.

Additional production GPU lifecycle coverage passes for water and lava at the
signed (-1,-1) four-column corner: cardinal/diagonal arrival, diagonal level and
above-fluid edits, partial unload, and complete halo unload. Every settled retained
mesh matches the independent unit-surface and fluid-data oracle, and corner heights
are asserted to change and return to their standalone value. Evidence:
build/voxel-halo-lifecycle-gpu.log. This final run also reruns all preceding GPU
mesh, raster, binding, precision and culling cases successfully.

## White seam flicker - 2026-09-13

Reproduced 71 exposed interior pixels across 256 moving oblique views with material
effects disabled. Precision-only changes did not fix these greedy T-junction
cracks. The production Slang mesher now emits a compact patch table; SlangRHI draws
unit quads or center-fan patches with matching unit-length boundary segments.
Face records and visible surfaces remain exact, with no inflation or LOD.

The final GPU suite passes with zero holes across 2,288,695 reference interior
pixels plus an independent analytic floor coverage test. Patch mapping/range,
materials, cutouts, fluids, culling and lifecycle checks pass. Packaged terrain,
fluid and complete 4,225-column runs exit normally. The repair adds raster work:
the short radius-4 sample rose from 0.575 to 0.754 ms and tracked storage grew 3.92 MB.
See voxel-seams.md for source ownership, evidence and limits. Streaming hitches
and broader engine parity remain unfinished.

### Atlas filtering and exact voxel batching — 2026-09-13

Repaired unbiased albedo minification with trilinear/8x anisotropic sampling,
crisp nearby pixels, sprite edge clamping and two mip alpha-coverage defects.
Packed LabPBR categories remain discrete. Optimized patch-coordinate arithmetic
is bit-exact across 202,374 tested vertices and preserves the seam repair.
Opaque/sprite draws now use bounded, capability-gated SlangRHI multi-draw with
retained mesh buffers; forward ordering remains unchanged. All shaders remain
Slang and full voxel detail is retained.

The final combined graphics suite passes, including 10,460 filtering samples,
independent face and fluid oracles, exact batched MRT/depth parity and 256 seam
views on each draw path with zero interior holes. Packaged fluid/GUI validation
and the complete max-distance surface capture pass. Current package/source and
826-file ownership/line-limit checks pass. See atlas-filtering.md and
voxel-batching.md for qualifications, provenance and exact evidence.

At 1280x720 and distance 32, the same-binary local OFF/ON comparison measured
13.946 -> 7.832 ms mean frame time and 7.787 -> 2.627 ms combined CPU draw
preparation/encoding/submission, retaining identical full-world mesh counts.
Periodic batch stdout flushing was removed; diagnostics use the existing GPU
CSV. These short measurements do not eliminate synchronous streaming hitches
or establish Linux/macOS or lower-capability fallback qualification.

### Chunk streaming latency and scratch reuse — 2026-09-13

Moved boundary mesh refresh into two bounded SlangRHI count/emit jobs. Current
source identities and prior visible mesh ownership gate publication; stale and
evicted results cannot resurrect geometry. Initial delivery keeps query/visible
coherence. Three jobs retain bounded scratch, cutting subsequent same-height
work from eleven buffer creations to four independently owned output buffers.
Stream payload destruction occurs outside the residency mutex, contiguous halo
decoding replaces repeated compact lookups, and animated atlas mip bytes are
cached without a separate per-animation queue wait.

Canonical GPU/batch, CPU storage/residency/draw, package/source and 833-file
ownership/line-limit checks pass. Twenty exact-fence lifecycle repeats and the
packaged distance 4 -> 8 -> 4/UI/surface capture pass. The test waits each actual
phase fence with a finite bound; production polls without blocking. Earlier
queue-idle fixture timeouts were preserved and investigated, not counted as a
production driver fix.

Matched visible 720p maximum-distance runs retain the same 15,223,536 rectangles.
Boundary backlog completion improves from about 122 to 96 seconds; loading
in-frame mesh CPU mean drops from 7.985 to 2.915 ms. Settled GPU time is unchanged.
A separate hidden 1440p run averages 9.906 ms in its final settled sample.
Initial delivery/frame waits and occasional CPU hitches remain. See
voxel-streaming.md for evidence, original-source mapping and qualification limits.

## Deeper no-LOD throughput qualification

Frame overlap and ordinary halo coalescing are implemented. The original greedy
shader is retained: a cooperative-output experiment passed geometry checks but
regressed solid/stepped GPU timing, so it was rejected. See
[voxel-throughput.md](voxel-throughput.md) for source reasoning, online references,
exact evidence, memory cost and next candidates.

The preserved Vulkan build improves the matched hidden radius-32 1440p settled
benchmark from 10.779 to 4.863 ms (~93 to 206 FPS), with identical recorded
4,225-column/15,221,650-rectangle geometry. Jobs fall 13,174 to 8,446 and stale
halo discards 201 to 0. Initial residency is slightly slower; complete boundary
work finishes earlier. This costs about 141 MiB extra tracked GPU memory.
CPU validation, two-slot GPU ownership, exact meshing, batch/seam tests,
packaged distance 4 -> 8 -> 4 and fluid/UI synchronization validation pass.
Loading hitches, synchronous initial delivery and sustained travel remain work.

A concurrent user-authorized task switched the live backend to DX12 through
Slang RHI. These measured Vulkan figures belong to the preserved package;
they do not establish DX12 FPS, where batching currently uses the existing
per-column draw path. Do not replace that live package with the benchmark copy.
