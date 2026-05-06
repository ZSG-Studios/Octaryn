# Octaryn Done / Not Done Ledger

Last checked: 2026-05-06.

This file is a status ledger. It does not override
`docs/architecture/octaryn-cpp-engine-systems-finish-plan.md`. "100% done"
below means the named file or area is complete for the current finish-plan
restriction that applies to it. The whole runtime is not done until the finish
plan completion definition is true.

## 100% Done For Current Restriction

### Client radius-32 stream batching guard

These files now satisfy the guarded behavior that replaced the old whole-stream
synchronous server-stream mesh update blocker:

- `octaryn-client/Source/App/WorldMeshRuntime/WorldMeshRuntime.cpp`
- `octaryn-client/Source/App/WorldMeshRuntime/WorldMeshRuntime.h`
- `octaryn-client/Source/App/FrameLoop/FrameLoop.cpp`
- `octaryn-client/Source/App/WorldStream/WorldStream.cpp`
- `octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry/TerrainMeshBatch.cpp`
- `octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry/EmptyWorldMesh.h`
- `octaryn-client/Source/Rendering/EmptyWorldMesh/Planning/ChunkMeshPlan.cpp`
- `octaryn-client/Source/Rendering/EmptyWorldMesh/Planning/ChunkMeshPlan.h`
- `octaryn-client/Source/Rendering/WorldMeshUpload/WorldMeshUpload.cpp`
- `octaryn-client/Source/App/Rendering/ShaderWorldPass/ShaderWorldPass.cpp`

Current done behavior:

- Server-stream mesh work is split into persistent per-frame batches.
- `TerrainMeshBatch` selects chunk/plan entries instead of requiring a full
  stream mesh rebuild.
- CPU build work is submitted through `octaryn_native_jobs` scheduled runtime.
- GPU upload application stays on the client main thread.
- Retained uploads, indirect draw, mipmaps, render-distance far plane, and
  no-LOD behavior remain guarded by the finish plan and request buffer.
- Existing runtime proof shows radius-32 `server_seed_memory` batching,
  metadata-only server stream records, unchanged-window skipping, and stable
  indirect draw.
- The live frame path polls the server chunk stream, computes the render-distance
  chunk view, calls the bounded mesh update path, and presents retained mesh
  buffers.

Proof artifacts:

- `logs/client/octaryn_client_radius32_runtime_proof.log`: 198 bounded
  `server_seed_memory` batches with radius 32.
- `logs/client/octaryn_client_radius32_runtime-proof.log`: 35 bounded
  `server_seed_memory` batches, final radius 32, and steady frame profile around
  105 FPS.
- `logs/client/octaryn_client_radius32_async-debug-linux.log`: 35 bounded
  `server_seed_memory` batches, final radius 32, and steady frame profile around
  107 FPS.
- `logs/server/octaryn_runtime_32_probe_after_hash_batches_stdout.log`: flat
  server proof log for radius 32 metadata-only stream behavior and unchanged
  window skipping.
- `logs/client/octaryn_client_app_launch_probe-debug-linux.log`: fresh launch
  probe proof for the direct pipeline, native scheduled runtime, retained
  upload, and direct-indirect draw. This is not radius-32 proof.

Important maintenance note:

- `WorldMeshRuntime.cpp` is 498 physical lines. It is under the hard 500-line
  limit, but any future behavior added there must split the file first.

### Client C# owner files

The remaining client C# files are acceptable for the current restriction because
they are module activation, validation, bundled-module lookup, command sink, or
host bridge import/export glue:

- `octaryn-client/Source/Host/Modules/BundledModuleCatalog.cs`
- `octaryn-client/Source/Host/Modules/BundledModuleLoader.cs`
- `octaryn-client/Source/Host/Modules/GameModuleActivator.cs`
- `octaryn-client/Source/HostBridge/Commands/HostCommandSink.cs`
- `octaryn-client/Source/HostBridge/Exports/HostExports.cs`
- `octaryn-client/Source/HostBridge/NativeLoading/NativeHostApi.cs`
- `octaryn-client/Source/Validation/ModuleValidation.cs`

Do not add client C# rendering, mesh, upload, scheduler, stream parsing, or
other engine-system ownership back here.

### Current source size rule

The active source/code tree currently has no file over 500 physical lines.

### Server host startup policy

These files now satisfy the current restriction for the server host startup
policy seam:

- `octaryn-server/Source/Host/HostPolicy.cpp`
- `octaryn-server/Source/Host/HostPolicy.h`
- `octaryn-server/Source/Host/Host.cs`
- `octaryn-server/Source/Host/NativeHostPolicyLibrary.cs`
- `octaryn-server/Source/Host/NativeHostPolicyTypes.cs`
- `tools/Source/ServerHostPolicyProbe/ServerHostPolicyProbe.cpp`

Current done behavior:

- Native server host code owns module-disable environment selection, live
  process-stream mode selection, live-loop interval policy, and startup
  `HostFrameSnapshot` construction.
- Native server host code owns live process-stream environment flag parsing,
  including the chunk-stream metadata-only mode used by the process bridge;
  managed process-stream code consumes that flag through native live-stream
  policy instead of naming or parsing the environment variable.
- Native server host code owns live process-stream path environment lookup for
  chunk-view intent, chunk-stream output, player input, block interaction, and
  world-time intents.
- Native server host code owns the live process-stream callback loop, stop
  result propagation, and interval wait; managed `Host` supplies the chunk
  stream iteration callback instead of owning the `while` loop.
- Managed `Host` keeps process/module orchestration plus existing ready,
  shutdown, and live-debug log contracts.
- `octaryn_server_host` is copied into the server bundle and injected into
  launch validation with `OCTARYN_SERVER_HOST_LIBRARY`.
- The native probe validates environment policy, live stream path lookup,
  startup frame ABI values, and live process-stream callback loop stop behavior.

### Server authority tick ordering

These files now satisfy the current restriction for the player/world-time
authority tick ordering seam:

- `octaryn-server/Source/Tick/AuthorityTick.cpp`
- `octaryn-server/Source/Tick/AuthorityTick.h`
- `octaryn-server/Source/Tick/AuthorityTickRunner.cs`
- `octaryn-server/Source/Tick/NativeAuthorityTickLibrary.cs`
- `octaryn-server/Source/Modules/ModuleActivator.cs`
- `tools/Source/ServerAuthorityTickProbe/ServerAuthorityTickProbe.cpp`

Current done behavior:

- The server-owned native authority tick plan schedules client command drain,
  player simulation, and world-time advancement in that order through
  `octaryn_native_jobs` and validates the expected schedule report shape.
- Managed `AuthorityTickRunner` is interop glue that supplies callbacks and asks
  native authority-tick owner code to validate the schedule report.
- The native probe validates ordering, dependency waves, worker routing,
  schedule-report validation, and callback failure short-circuiting.

### Server world-time process intent planning

These files now satisfy the current restriction for world-time speed intent
read-result application planning:

- `octaryn-server/Source/World/Time/WorldTimeIntent.cpp`
- `octaryn-server/Source/World/Time/Clock.h`
- `octaryn-server/Source/World/Time/NativeWorldTimeLibrary.cs`
- `octaryn-server/Source/World/Time/NativeWorldTimeTypes.cs`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`
- `tools/Source/ServerWorldTimeProbe/ServerWorldTimeProbe.cpp`

Current done behavior:

- Native world-time code maps missing, invalid, and unsupported speed-intent
  reads into process application plans.
- Native world-time code owns speed-intent process stop-reason labels for the
  existing live-log contract.
- Managed `ChunkStreamProcessBridge` keeps speed multiplier application and the
  existing `server_live_world_time_intent` log strings.
- Missing optional speed-intent files still produce no live-log line.
- The native probe validates unsupported and missing stop-reason labels.

### Server world-block persistence orchestration

These files now satisfy the current restriction for the aggregate world-block
override persistence seam:

- `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.h`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldOverrides/WorldBlockPersistence.cs`
- `tools/Source/ServerWorldPersistenceProbe/WorldBlockPersistencePolicyProbe.cpp`

Current done behavior:

- Native world-persistence code selects aggregate vs. chunk-column sidecar load
  sources, preserving current sidecar freshness and newer aggregate precedence.
- Native world-persistence code owns world-block initialization, dirty save
  flushes, empty-save aggregate deletion, stale sidecar pruning, and
  aggregate/sidecar write coordination.
- Native world-persistence code owns world-block dirty tracking and
  save-needed decisions; managed persistence marks dirty and passes snapshots
  only after the native owner reports that a save is due.
- Managed `WorldBlockPersistence` keeps only environment path selection,
  block-store snapshot/load handoff, and native interop conversion.
- Managed `SaveExportBundleFile` imports chunk DTOs by validating bundle
  versions, converting DTO blocks to native edits, and calling the same native
  aggregate/sidecar save coordinator instead of writing `world_blocks.json`
  directly.
- Managed `SaveExportBundleFile` exports chunk DTOs from native persistence
  load-source readers and the native chunk-column planner instead of replaying
  world-block saves through a managed `BlockStore`.
- Native world-persistence probe coverage directly validates save-export chunk
  column planning for active aggregate saves and chunk-column-only saves.
- Native world-persistence code exposes active world-block override read
  count/fill APIs so managed `WorldBlockPersistence` no longer selects
  aggregate versus chunk-column sidecar sources before loading edits.
- Native world-persistence code also counts aggregate `world_blocks.json`
  chunk columns for save metadata without loading block records into managed
  metadata code.

### Server player directory persistence

These files now satisfy the current restriction for the player-directory
persistence seam:

- `octaryn-server/Source/Persistence/WorldBlocks/PlayerDirectory.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.h`
- `octaryn-server/Source/Persistence/WorldBlocks/NativePlayerDirectoryPersistence.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeWorldPersistenceLibrary.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeWorldPersistenceTypes.cs`
- `octaryn-server/Source/Persistence/Players/PlayerPersistence.cs`
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`
- `octaryn-server/Source/Persistence/WorldSave/WorldSaveMetadataBuilder.cs`
- `tools/Source/ServerWorldPersistenceProbe/PlayerDirectoryProbe.cpp`

Current done behavior:

- Native world-persistence code scans `player_<id>.json` files, parses player
  IDs, ignores unsupported/corrupt player saves, loads valid native player
  state, and returns entries sorted by player ID.
- Native world-persistence code writes imported player entries through the same
  `player_<id>.json` filename/path policy.
- Ordinary managed `PlayerPersistence` load/save calls now pass a player ID and
  native player state to the native player-directory API instead of constructing
  the load/save file path in C#. `PathFor` remains as a diagnostic path-shape
  helper covered by the persistence probe.
- Managed `SaveExportBundleFile` now maps native player entries into the
  existing export DTO and passes player import state back to native
  player-directory persistence instead of owning directory enumeration, JSON
  load filtering, or player filename/path construction.
- Managed `WorldSaveMetadataBuilder` now gets valid player-save counts from the
  same native directory API and aggregate chunk-column counts from native
  world-persistence instead of owning filename parsing, player JSON load
  filtering, or aggregate world-block count loading.

### Server chunk-column override store shim

These files now satisfy the current restriction for removing the managed
chunk-column store shim:

- `octaryn-server/Source/Persistence/WorldBlocks/ChunkColumns/ChunkColumnOverrideStore.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldOverrides/WorldBlockPersistence.cs`
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`
- `octaryn-server/Source/Persistence/WorldSave/WorldSaveMetadataBuilder.cs`
- `tools/validation/Octaryn.ServerPersistenceProbe/Program.cs`

Current done behavior:

- The managed `ChunkColumnOverrideStore` file is deleted.
- `WorldBlockPersistence`, `SaveExportBundleFile`, and
  `WorldSaveMetadataBuilder` call native world-persistence planning, directory
  scanning, and load-source APIs directly.
- The managed server persistence probe keeps local probe-only chunk-column file
  helpers so validation can inspect the legacy JSON shape without reintroducing
  production store ownership.

### Server terrain generation sampling

These files now satisfy the current restriction for server generated-block
sampling:

- `octaryn-server/Source/World/Generation/TerrainGeneration.cpp`
- `octaryn-server/Source/World/Generation/TerrainGeneration.h`
- `octaryn-server/Source/World/Generation/TerrainGenerator.cs`
- `octaryn-shared/Source/World/Terrain/IWorldGenerationRules.cs`
- `octaryn-shared/Source/World/Terrain/TerrainMaterialRules.cs`
- `octaryn-basegame/Source/Content/Worldgen/WorldGenerationRules.cs`
- `tools/Source/ServerTerrainGenerationProbe/ServerTerrainGenerationProbe.cpp`

Current done behavior:

- Native terrain-generation code owns old-architecture column height/material
  planning, floor chunk-local mapping, water/surface/fill selection,
  world-bounds checks, and empty-world sampling.

### Server chunk-column request availability

These files now satisfy the current restriction for the chunk-column request
availability seam:

- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnRequest.cpp`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.h`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStreamProvider.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreLibrary.cs`
- `octaryn-server/Source/Modules/ModuleActivator.cs`
- `cmake/Owners/ToolTargets/ToolNativeTargets.cmake`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreChunkRequestProbe.cpp`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreChunkStreamProbe.cpp`

Current done behavior:

- Native chunk-stream code owns the unavailable-terrain request status for
  module-disabled hosts or hosts without a generated-block source.
- Managed `ChunkColumnStreamProvider` keeps only native interop and the existing
  `server_live_chunk_request` success log.
- The native block-store probe validates both unavailable and available request
  gates.
- Managed `TerrainGenerator` marshals stable material block rules from the
  module registration once and no longer calls back into managed
  `PlanTerrainColumn` for generated-block samples.

### Server player input process planning

These files now satisfy the current restriction for process player-input intent
read-result stop planning:

- `octaryn-server/Source/Simulation/Players/PlayerInputIntent.cpp`
- `octaryn-server/Source/Simulation/Players/PlayerSimulation.h`
- `octaryn-server/Source/Simulation/Players/NativePlayerSimulation.cs`
- `octaryn-server/Source/Simulation/Players/NativePlayerSimulationTypes.cs`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`
- `tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationProbe.cpp`

Current done behavior:

- Native player-simulation code maps missing, retry, partial, unsupported, and
  failed player-input intent reads into process stop/continue/tick plans.
- Native player-simulation code owns player-input process stop-reason labels for
  the existing live-log contract.
- Managed `ChunkStreamProcessBridge` keeps `HostFrameSnapshot` conversion and
  existing `server_live_player_input_intent` log strings.
- The native probe validates successful tick planning, unsupported-intent stop
  planning, and native-owned reason labels.

### Server player session save bookkeeping

These files now satisfy the current restriction for player session state and
save-cadence bookkeeping:

- `octaryn-server/Source/Simulation/Players/PlayerStateLoad.cpp`
- `octaryn-server/Source/Simulation/Players/PlayerSimulation.h`
- `octaryn-server/Source/Simulation/Players/NativePlayerSimulation.cs`
- `octaryn-server/Source/Simulation/Players/NativePlayerSimulationTypes.cs`
- `octaryn-server/Source/Simulation/Players/PlayerController.cs`
- `tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationSaveProbe.cpp`

Current done behavior:

- Native player-simulation code owns the player session struct, current state,
  last saved state, loaded-from-save flag, elapsed save timer, save decisions,
  and successful-save acknowledgement.
- Native player-simulation code owns player control-mode live-log label mapping
  so managed controller logging does not duplicate mode-name policy.
- Managed `PlayerController` keeps persistence invocation, snapshots, and
  existing live-log text while delegating spawn alignment, tick stepping, and
  save bookkeeping to native session APIs.
- The native probe validates session creation, unchanged elapsed-save tracking,
  changed-state save release, save-state projection, and saved-baseline updates.

### Server block command application policy

These files now satisfy the current restriction for native set-block command
application:

- `octaryn-server/Source/World/Blocks/Store/BlockEditService.cpp`
- `octaryn-server/Source/World/Blocks/Store/BlockEditService.h`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreCommandValidationProbe.cpp`

Current done behavior:

- Native block-store code owns set-block command preflight and application
  through the same client-interaction, block-known, support, generated-block,
  unchanged, preserved-air, and cascade-clearing policy path.
- Managed `BlockEditService.ApplyCommand` is interop glue over the native
  apply-command ABI.
- The native probe validates command apply success, unknown-block rejection,
  occupied client-interaction rejection, and unsupported-block cascade clearing.

### Server block interaction process intent planning

These files now satisfy the current restriction for block-interaction intent
read-result and duplicate-frame process planning:

- `octaryn-server/Source/World/Blocks/Commands/Interaction/BlockInteractionFrameTracker.cpp`
- `octaryn-server/Source/World/Blocks/Commands/Interaction/BlockInteractionIntent.cpp`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.h`
- `octaryn-server/Source/World/Chunks/Streaming/NativeChunkColumnStreamTypes.cs`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreLibrary.cs`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreChunkStreamProbe.cpp`

Current done behavior:

- Native block-interaction intent parsing returns command count, break count,
  and place count with the parsed host-command frame.
- Native block-store code maps missing, retry, partial, unsupported, and failed
  block-interaction intent reads into process stop/continue plans.
- Native block-store code folds duplicate-frame tracker state into the same
  process plan so managed code no longer owns duplicate-frame stop planning.
- Native block-store code owns block-interaction process stop-reason labels for
  the existing live-log contract.
- Managed `ChunkStreamProcessBridge` logs those native summary counts and keeps
  only path, transient-error, submission, and live-log glue for that intent.

### Server chunk-stream process intent read

These files now satisfy the current restriction for process chunk-view intent
read/result mapping:

- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamWriteTracker.cpp`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.h`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreLibrary.cs`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreChunkStreamProbe.cpp`

Current done behavior:

- Native chunk-stream code reads the process chunk-view intent and maps
  missing, retry, partial, unsupported, and failed reads into the existing
  process write-plan reason/handle-result contract.
- Native chunk-stream code owns process write-plan stop-reason labels for the
  existing live-log contract.
- Managed `ChunkStreamProcessBridge` no longer owns chunk-view intent file
  existence checks or read-result mapping; it keeps environment path lookup,
  live-log text, player/block/world-time glue, tick callbacks, and snapshot
  write orchestration.
- The native probe validates successful process-intent reads plus unsupported
  and missing-intent stop plans.

### Server client block command queue submit/drain

These files now satisfy the current restriction for client block command queue
submit reporting and draining:

- `octaryn-server/Source/World/Blocks/Store/BlockCommandQueue.cpp`
- `octaryn-server/Source/World/Blocks/Store/BlockCommandQueue.h`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreLibrary.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreTypes.cs`
- `octaryn-server/Source/World/Blocks/Commands/Queue/ClientBlockCommandQueue.cs`
- `octaryn-server/Source/World/Blocks/Commands/Queue/BlockCommandSink.cs`
- `octaryn-server/Source/World/Blocks/Commands/Editing/BlockEditService.cs`
- `octaryn-server/Source/Modules/ModuleActivator.cs`
- `octaryn-shared/Source/Host/Commands/HostCommand.cs`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreCommandQueueProbe.cpp`

Current done behavior:

- Native block-store code owns host-command current ABI version/size validation
  for server block-command enqueue checks; the shared `HostCommand` contract no
  longer carries a managed validation helper.
- Native block-store queue code returns the submit report shape for client
  command frames: result, reason, rejected index, requested count, and
  before/after pending counts. Managed module activation consumes that report
  to preserve existing live-log text without owning submit result planning.
- Native block-store code owns block-command edit live-log labels (`break`,
  `place`, `none`); the managed `BlockCommandDiagnostics` helper is deleted.
- Native block-store code drains queued client block commands and applies them
  through the same native set-block command policy used by direct server
  commands.
- Managed `ClientBlockCommandQueue` keeps allocation, submit, pending-count,
  dispose, and live-log glue only; it no longer owns drain-time command
  application callbacks.
- Managed `BlockCommandSink` still owns live-log output and persistence dirty
  notification after native apply results are returned.
- The native probe validates submit rejection, drain-apply success, drain-apply
  rejection, store mutation, callback result counts, and queue clearing.

### Server save-export gzip file lifecycle

These files now satisfy the current restriction for save-export gzip file
lifecycle ownership:

- `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistenceGzip.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeGzipPersistence.cs`
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`
- `tools/Source/ServerWorldPersistenceProbe/GzipPersistenceProbe.cpp`
- `tools/validation/Octaryn.ServerPersistenceProbe/Program.cs`

Current done behavior:

- Native world-persistence code creates parent directories for gzip exports,
  writes the compressed payload through a `.tmp` path, and replaces existing
  export files.
- Managed `SaveExportBundleFile.SaveGzip` keeps JSON DTO serialization only and
  passes the payload/final path to native persistence for gzip file I/O.
- The server persistence probe validates gzip export write, temp cleanup,
  existing-file overwrite, reload, and import behavior.

## Not Done

### Whole finish plan

The finish plan is not complete. Current active C# file counts:

- `octaryn-client`: 7
- `octaryn-server`: 67
- `octaryn-shared`: 79
- `octaryn-basegame`: 13

The client count is acceptable for current glue/API restrictions. The server
count is not.

### Server C# owner-system blockers

These server files and areas still own too much engine/runtime behavior and must
continue moving into focused C++ owner code:

- `octaryn-server/Source/Modules/ModuleActivator.cs`: runtime composition,
  persistence save calls, module tick command-write callback, and snapshot drain
  orchestration; client command drain ordering is native-scheduled.
- `octaryn-server/Source/Host/Host.cs`: live process loop and sleep-driven
  stream handling.
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`:
  process-stream request handling, intent file/env orchestration, ticking,
  snapshotting, and stream output decisions; block-interaction command
  summaries/read-result/duplicate-frame planning are native.
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`: save
  export/import DTO and chunk bundle orchestration; gzip file lifecycle, player
  directory scan/load filtering, chunk export loading/planning, and world-block
  import writes are native.
- `octaryn-server/Source/Simulation/Players/PlayerController.cs`: player state
  ownership, save cadence calls, persistence invocation, and logging around
  native stepping.
- `octaryn-server/Source/World/Blocks/Commands/Queue/BlockCommandSink.cs`:
  managed command logging/enqueue orchestration around native block-command
  application; current ABI validation is native-owned.

### Shared and basegame boundary

- `octaryn-shared` must remain API/contracts/validation/sandbox policy only.
  Any storage, scheduler, persistence, stream, simulation, render, or host
  implementation type is not done.
- `octaryn-basegame` may stay C# only as bundled gameplay/content module logic
  using shared contracts. It must not gain client rendering, server
  persistence, transport, native bridge internals, raw threading, filesystem, or
  host service ownership.

### Runtime/profiling proof gaps

Existing logs prove bounded radius-32 behavior, but the ledger still needs fresh
direct runtime proof for:

- 32 chunk render distance loading in the 3-6 second target window.
- Full visual/culling correctness under current artifacts.
- Tracy or RenderDoc evidence when doing FPS/lag work.

### Cleanup blockers

No known empty active source folders are currently blocking the finish-plan
cleanup rule.

## Current Validation Baseline

Use targeted probes, not `ctest` and not smoke tests. The current baseline to
rerun after relevant changes is:

- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_chunk_mesh_plan_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_empty_world_mesh_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_jobs_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_host_policy_native_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_authority_tick_native_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_owner_boundaries`
- server native-owner probes matching the touched server slice.
