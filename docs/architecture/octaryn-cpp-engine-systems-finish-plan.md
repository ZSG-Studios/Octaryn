# Octaryn C++ Engine Systems Finish Plan

## End Goal

The active Octaryn runtime must be a C++ engine with managed code limited to the game/module API boundary. Client, server, and shared owners must use Octaryn native libraries for jobs, memory, diagnostics, profiling, shader tooling, atlas/mesh packing, GPU upload preparation, persistence backends, and validation. C# may remain only for module contracts, manifests, validation policy, module activation glue, and host bridge exports/imports until those bridge seams have a clean C++ owner replacement.

The finished runtime must restore old-architecture behavior and performance without adding LODs:

- 32 chunk render distance streams and renders in 3-6 seconds on the target machine.
- Terrain is full-depth, correctly culled, and does not produce one-layer columns or missing chunk bodies.
- Seed terrain data stays memory/VRAM only; only authoritative server edits persist or stream as block override data.
- Chunk generation, chunk meshing, stream parsing, mesh packing, and upload preparation do not block the main render thread.
- The client uses retained GPU resources, mipmapped material sampling, indirect rendering, and render-distance-driven far plane behavior.
- Server authority remains intact for edits, validation, persistence, simulation, and replication.

## Current State

Completed in the current cleanup pass:

- Removed managed client world presentation, meshing, block render catalog, snapshot consumer, mesh packing, and mesh upload planning code.
- Removed the managed client/server `HostScheduler` implementations, managed scheduler shared internals, scheduler CMake targets, scheduler probe project, scheduler contract validator, and stale solution references.
- Client/server module activators no longer run module ticks through C# scheduler worker/coordinator code.
- Moved module command-write scope state behind the loadable `octaryn_native_jobs` owner gate; managed client/server module ticks now call thin native scope glue instead of owning scheduler state.
- Routed client/server module tick callbacks through the native scheduled-runtime bridge so command-write authorization is opened by `octaryn_native_jobs` runtime execution instead of direct managed scope entry.
- Added a native jobs validation probe that exercises worker-policy limits, native scheduler resource/main-thread policy, native command-write scope gating, native scheduled-runtime wave execution, and Taskflow dependency/barrier execution through the C++ native jobs target.
- Moved live server world-time clock/calendar/blob/frame tick execution into the server-owned native world-time library; managed server code now only holds native handle/interoperability glue for the module-facing world-time contract.
- Moved world-time speed intent file read/validation into the server-owned native world-time library; managed process-stream code now only maps native intent results to existing live-log outcomes before applying the speed multiplier.
- Moved world-time speed intent read-result application planning into the server-owned native world-time library; managed process-stream code now keeps speed application and existing live-log strings only.
- Moved world-time speed multiplier state and clamping into the server-owned native world-time clock; managed server code now passes real frame delta and calls native speed-multiplier interop only.
- Added a server-owned native block override store/change queue and native probe, mapped from the current server override/replication-change semantics and old local chunk indexing constraints, to start moving server block storage out of managed engine-system code.
- Moved server block-edit apply/unchanged/preserved-air/cascade support behavior into the native server block-store library; managed `BlockEditService` is now interop glue around native edit policy callbacks.
- Moved client block-interaction hit-position, reach, and break/place adjacency validation into the native server block-store command policy; managed command sink now supplies block lookup/basegame authority glue.
- Moved block command validation lookup into the native server block-store command policy; managed command sink now delegates set-block preflight to native code and supplies only authority-rule callbacks.
- Moved set-block command application into the native server block-edit service; managed command sink now delegates command-to-edit conversion and apply policy to native code before logging/enqueuing returned changes.
- Moved process block-interaction intent file read/validation and command ABI packing into native server block-store owner code; managed process-stream code now submits the native-filled command buffer.
- Moved process block-interaction frame duplicate gating into native server block-store owner code; managed process-stream code now asks the native tracker before submitting a frame and records submitted frames after successful queue handoff.
- Moved process block-interaction intent read-result mapping and duplicate-frame stop planning into native server block-store owner code; managed process-stream code now keeps environment, live-log text, and command submission glue for that intent.
- Moved process block-interaction stop-reason label mapping into native server block-store owner code; managed process-stream code now logs native-owned reason names.
- Moved client block-command queue drain-time application into native server block-store owner code; managed queue code now keeps allocation, submit, pending-count, disposal, and live-log glue around native apply results.
- Moved player save, world-time, and world metadata JSON read/write into the native server world-persistence library; managed persistence now keeps root selection, diagnostics, metadata, and export-bundle DTO glue only.
- Moved save-export and world-metadata player file enumeration, filename ID parsing, valid-player filtering, native player JSON loading, count reporting, and export ordering into the native server world-persistence library; managed save-export/metadata code now maps native player entries/counts into existing DTOs.
- Moved save-import player filename/path construction into the native server world-persistence player-directory API; managed save import validates DTO versions and passes native player state to owner code.
- Moved ordinary `PlayerPersistence` load/save player filename construction into the native player-directory API; managed persistence keeps `PathFor` only as a diagnostic path-shape helper.
- Moved chunk-column override JSON read/write and legacy coordinate upgrade into the native server world-persistence library; managed chunk-column files now keep DTO/export glue and directory orchestration.
- Moved aggregate world-block override JSON read/write into the native server world-persistence library; managed world-block files now keep DTO/export glue only.
- Moved chunk-column override directory load/freshness/count scanning into native server world-persistence code; managed chunk-column store now keeps allocation, path, and interop orchestration only for those decisions.
- Moved stale chunk-column override file pruning, sidecar directory writes, and `chunk_<cx>_<cz>.json` filename/path handling into native server world-persistence code; managed chunk-column store now passes the native column plan to owner code.
- Moved world-block load-source selection, initialization, dirty flush, aggregate deletion, and aggregate/sidecar save coordination into native server world-persistence code; managed world-block persistence now keeps path and block-store interop glue.
- Moved world-block dirty tracking and save-needed decisions into native server world-persistence code; managed world-block persistence now asks the native owner whether a save is required before passing snapshots to the native save backend.
- Moved save-import world-block aggregate/sidecar write coordination into native server world-persistence code; managed save-export code now validates chunk DTO versions and passes native edits to the owner save API.
- Moved world-save metadata aggregate chunk-column counting into native server world-persistence code; managed metadata code now asks the native owner for valid player counts and aggregate chunk-column counts.
- Removed the managed chunk-column override store shim; save export, metadata, world-block persistence, and managed probes now call native world-persistence directory planning/scanning APIs directly at their remaining DTO/probe edges.
- Added client-owned native chunk mesh planning for streamed/empty terrain updates, with old window-overlap preserve/load/unload accounting, center-priority job ordering, retained-upload logging, and a Taskflow-backed native probe.
- Removed the managed client chunk-mesh upload drain export and bridge/probe callers; live client terrain mesh updates now stay on native `WorldMeshRuntime` server/empty-world scheduled build/upload paths.
- Moved chunk stream snapshot writing into native server code and removed managed chunk stream capture construction; managed server code now requests native stream snapshot writes through interop glue.
- Made native chunk stream load/preserve/unload event output optional so callers can avoid unneeded event payloads.
- Moved chunk-stream metadata write-window tracking and duplicate unchanged-window skip decisions into native server chunk-stream owner code.
- Moved server chunk-view intent file read/validation into native server chunk-stream owner code; managed process-stream code now keeps only path/env orchestration for that intent.
- Moved process chunk-view intent read-result mapping into native server chunk-stream owner code; managed process-stream code now consumes native stop/continue plans for missing, retry, partial, unsupported, and failed intent reads.
- Moved process chunk-view write-plan stop-reason label mapping into native server chunk-stream owner code; managed process-stream code now logs native-owned reason names.
- Added bounded per-frame server-stream mesh batching in client owner code, selected-entry `TerrainMesh` construction, and native-job CPU build/packing while keeping GPU upload application on the client main thread.
- Moved player collision block-store lookup into the native server player simulation path; managed player simulation glue now supplies only generated-block and solidity callbacks.
- Moved server player input-intent detection into the native player simulation owner library; managed controller code now asks native owner code whether to run movement or idle.
- Moved process player-input intent file read/validation into the native player simulation owner library; managed process-stream code now keeps HostFrame conversion/logging glue only.
- Moved process player-input intent read-result stop planning into the native player simulation owner library; managed process-stream code now keeps HostFrame conversion and live-log glue for that intent.
- Moved process player-input stop-reason label mapping into the native player simulation owner library; managed process-stream code now logs native-owned reason names.
- Moved saved player state finite checks and clamp/normalization into the native player simulation owner library; managed controller code now treats save loading as persistence glue before native state construction.
- Moved player spawn-adjusted reporting into the native player simulation owner library; managed controller code now logs the native alignment result instead of owning a local movement epsilon.
- Moved player save-cadence policy and save-timer accumulation into the native player simulation owner library so live movement no longer writes player JSON every changed frame; managed controller code keeps only persistence invocation and log output.
- Moved server player tick idle-vs-move selection into the native player simulation owner library; managed controller code now calls one native step and keeps only persistence/log output.
- Moved server player movement delta reporting into the native player simulation step result; managed controller logging now consumes native step output instead of deriving movement deltas in C#.
- Moved server player save-state projection into the native player simulation owner library; managed controller now asks native code for the persisted player state before invoking persistence.
- Routed server authority tick work for player simulation and world-time advancement through the native scheduled-runtime worker path; managed activation now keeps ordering/log/persistence glue around native job execution.
- Moved server authority tick ordering for player simulation before world-time advancement into the server-owned native authority-tick schedule plan; managed `AuthorityTickRunner` now supplies callbacks and validates the native schedule report.
- Moved server client-command drain ordering into the same native authority-tick schedule plan, so client command drain, player simulation, and world-time advancement run as ordered `octaryn_native_jobs` worker callbacks before managed module tick/log/persistence glue.
- Moved server terrain column planning into the native terrain-generation library; managed server terrain generation now passes block material rules once and samples generated blocks without per-column managed callbacks.
- Routed server save-export chunk loading through native world-persistence readers and the native chunk-column planner; managed export code no longer replays saved chunk data through a managed `BlockStore` before building DTOs.
- Moved server host startup policy, live process-stream mode selection, live loop interval policy, and startup host-frame construction into a focused native server host library; managed `Host` now keeps module/process orchestration and live-log/ready-shutdown glue only.
- Moved live process-stream metadata-only environment flag parsing into the focused native server host library; managed chunk-stream process code now asks native host policy before applying metadata-only tick/write behavior.
- Moved server chunk-column request availability gating into native chunk-stream owner code; managed stream provider now passes generation availability and keeps only interop/log glue.
- Updated validation and docs so the deleted managed scheduler/client presentation probes are no longer active targets.

Validated after those removals:

- `octaryn_client_bundle`
- `octaryn_server_bundle`
- `octaryn_validate_hostfxr_bridge_exports`
- `octaryn_validate_cmake_targets`
- `octaryn_validate_owner_module_validation_probe`
- `octaryn_validate_module_source_api`
- `octaryn_validate_native_owner_boundaries`
- `octaryn_validate_native_jobs_probe`
- `octaryn_validate_client_chunk_mesh_plan_probe`
- `octaryn_validate_server_host_policy_native_probe`
- `octaryn_validate_server_world_time_native_probe`
- `octaryn_validate_server_authority_tick_native_probe`
- `octaryn_validate_server_block_store_native_probe`
- `octaryn_validate_server_world_persistence_native_probe`
- `octaryn_validate_server_player_simulation_native_probe`
- `octaryn_validate_server_world_blocks_probe`
- `octaryn_validate_client_app_launch_probe`
- `octaryn_validate_owner_launch_probes`
- direct client/server radius-32 runtime proof for bounded `server_seed_memory` batches, retained uploads, stable indirect draw, and metadata-only server stream churn
- `octaryn_validate_hostfxr_bridge_exports`
- `octaryn_validate_dotnet_owners`
- `git diff --check`
- Empty-folder scan
- Touched/active source line-count scan for files over 500 physical lines

Current managed source count across active owners:

- `octaryn-shared`: 79 C# files, mostly API/contracts/validation/sandbox policy.
- `octaryn-server`: 66 C# files, still too much owner system code plus native interop glue.
- `octaryn-basegame`: 13 C# files, acceptable only as module gameplay/content API use.
- `octaryn-client`: 7 C# files, mostly host bridge/module glue.

## Must Be 100% Finished

### 1. Native Jobs And Scheduling

- Implement the real owner scheduler in C++ using `octaryn_native_jobs`, the Taskflow wrapper, and existing profiling/logging/diagnostics.
- Enforce one main thread, one coordinator thread, and a scalable worker pool with at least two workers.
- Adopt the native scheduled-runtime API from client/server owner code for real frame/tick work routing under gameplay load.
- Expand focused native scheduler validation into runtime profiling/load evidence; current native jobs validation covers worker policy, dependencies, barriers, resource conflicts, command-write gating, scheduled-runtime wave execution, and no main-thread blocking policy.

### 2. Client Terrain Streaming And Meshing

- Preserve the bounded per-frame server-stream mesh batching in `WorldMeshRuntime` and the selected-entry `TerrainMesh` API; do not reintroduce whole-stream synchronous build/upload work.
- Finish old-architecture chunk streaming, terrain meshing, face culling, batching, and mesh packing parity in focused C++ owner files.
- Use native jobs for chunk stream parsing, seed terrain sampling, meshing, packing, and upload staging without blocking the render frame on the whole radius-32 stream.
- Keep GPU API calls and final presentation on the client main thread only.
- Preserve no-LOD behavior unless explicitly requested.
- Validate 32 chunk render distance loads within the 3-6 second target with profiling logs.

### 3. Client Rendering Performance

- Finish retained chunk GPU resources and indirect rendering as the default path.
- Confirm mipmaps are generated, uploaded, and sampled for block atlases without breaking nearest UI/composite sampling.
- Keep far plane tied to render distance instead of clipping terrain incorrectly.
- Add direct profiling logs for world update, meshing, upload, draw, sky, composite, and UI timing.
- Fix any remaining face-culling errors with old-architecture parity checks.

### 4. Server World Authority And Persistence

- Move remaining server world systems out of C# into C++ owner code where they are engine systems: world time, player simulation, block storage, terrain generation, chunk streaming, command queues, and persistence backends.
- Preserve server authority for edits and validation.
- Persist only edited/different block overrides and metadata. Never write seed/generated terrain block data to JSON or disk.
- Keep module-provided gameplay rules and content declarations behind shared API contracts.

### 5. Shared And Basegame API Boundary

- Keep `octaryn-shared` C# as API/contracts only: manifests, commands, snapshots, IDs, positions, module validation, capability/package allowlists, and narrow host API declarations.
- Keep `octaryn-basegame` C# only as bundled module gameplay/content logic using shared API contracts.
- Remove or convert any shared/server/client C# type that owns storage, scheduling execution, streaming, persistence implementation, simulation loops, or render data preparation.
- Keep denied APIs enforced for modules: no raw threading, networking stacks, filesystem, native interop, reflection loading, or host internals.

### 6. Validation And Runtime Proof

- Build `debug-linux` owner bundles and x64 native targets after each coherent slice.
- Run focused validation targets for CMake inventory, native owner boundaries, module API policy, host bridge exports, module validation, server persistence/world generation/world blocks, and launch probes that match the changed area.
- Run the Linux client/server runtime directly after performance slices and keep it open when requested.
- Capture profiling logs or Tracy/RenderDoc evidence for FPS/lag work instead of relying on smoke tests.
- Keep `ctest` out of this path unless explicitly requested.

## Completion Definition

This plan is complete only when:

- No client/server engine system remains implemented in C#.
- C# remaining in client/server/shared is demonstrably module API, validation policy, host bridge glue, or module activation glue.
- All hot terrain/chunk/render/server loops run through C++ owner code and approved Octaryn native libraries.
- 32 chunk render distance is stable, fast, and visually correct.
- Seed terrain never persists as chunk data.
- Indirect rendering, mipmaps, retained uploads, and render-distance far plane behavior are active and validated.
- Native jobs are proven to own heavy work off the render thread.
- All active source files remain under 500 physical lines and owner folders stay clean.
