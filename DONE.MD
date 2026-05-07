# Octaryn Done / Not Done Ledger

Last checked: 2026-05-07.

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
- Fresh direct runtime proof shows the current bundled server/offscreen client
  path reaches radius 32 as 4,225 columns through bounded batches, with max 24
  entries per batch, direct-indirect draw still active, and no seed terrain
  block records persisted.
- The live frame path polls the server chunk stream, computes the render-distance
  chunk view, calls the bounded mesh update path, and presents retained mesh
  buffers.

Proof artifacts:

- `logs/client/octaryn_client_radius32_final_runtime_proof.log`: offscreen
  bundled-server direct run with 198 bounded `server_seed_memory` batches, max
  `processed=24`, final complete radius-32 batch at 4,225 columns, retained
  direct-indirect draw, and frame profile at 133.8 average FPS with 108.1
  low-0.1 FPS after warmup.
- `logs/server/singleplayer-15535485/server_live.log`: server stream proof for
  radius 32, 4,225 columns, `blocks=0`, `metadata_only=1`, followed by
  unchanged-window skips. The matching session root did not write
  `world_blocks.json`.
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
- Native server host code owns live process-stream request gating for missing
  chunk-view intent and missing chunk-stream output path; managed process-stream
  code consumes native handle/continue/result plans and preserves the existing
  `server_live_chunk_stream` log contract.
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
- `octaryn-server/Source/Persistence/WorldBlocks/WorldMetadataPersistence.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldOverrides/WorldBlockPersistence.cs`
- `octaryn-server/Source/Persistence/WorldSave/WorldSaveMetadataBuilder.cs`
- `tools/Source/ServerWorldPersistenceProbe/WorldMetadataBuildProbe.cpp`
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
- Managed `WorldBlockPersistence` keeps only block-store snapshot/load handoff,
  native path-policy interop, and native edit conversion.
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
- Native world-persistence code builds world-save metadata from native
  world-time, player-directory, sidecar chunk-column, and aggregate
  `world_blocks.json` discovery; metadata discovery asks native path policy for
  root-relative `world_time.json` and `world_blocks.json` paths before reading.
  Managed metadata builder now maps that native result into the existing DTO.

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
  the load/save file path in C#. `PathFor` now calls the native
  player-directory path API, keeping the `player_<id>.json` path shape owned by
  native persistence and covered by the persistence probe.
- Managed `SaveExportBundleFile` now maps native player entries into the
  existing export DTO and passes player import state back to native
  player-directory persistence instead of owning directory enumeration, JSON
  load filtering, or player filename/path construction.
- Managed `WorldSaveMetadataBuilder` now delegates metadata discovery to native
  world-persistence instead of owning filename parsing, player JSON load
  filtering, or aggregate/sidecar world-block count composition.

### Server persistence path policy

These files now satisfy the current restriction for server persistence root and
sidecar path selection:

- `octaryn-server/Source/Persistence/WorldBlocks/PathPolicy.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.h`
- `octaryn-server/Source/Persistence/WorldBlocks/NativePathPolicyPersistence.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeWorldPersistenceLibrary.cs`
- `octaryn-server/Source/Persistence/Players/PlayerPersistence.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldOverrides/WorldBlockPersistence.cs`
- `tools/Source/ServerWorldPersistenceProbe/PathPolicyProbe.cpp`
- `tools/validation/Octaryn.ServerPersistenceProbe/Program.cs`

Current done behavior:

- Native world-persistence path policy owns the fallback from
  `OCTARYN_SERVER_WORLD_BLOCKS_PATH`, `OCTARYN_SERVER_PLAYER_SAVE_ROOT`, and
  `OctarynBuildPresetName` into the active world root, `world_blocks.json`, and
  player-directory paths.
- Native world-persistence path policy owns aggregate `world_blocks.json`
  sidecar directory derivation, including filename-only aggregate paths mapping
  to `.`.
- Native world-persistence path policy owns root-relative `world_time.json`,
  `world_blocks.json`, and `world_meta.json` path derivation for save/export
  and metadata edges.
- Managed `PlayerPersistence.FromEnvironment` and
  `WorldBlockPersistence.FromEnvironment` are interop callers over native path
  policy instead of composing persistence roots in C#.
- Managed `SaveExportBundleFile.FromWorldRoot` asks native path policy for
  world-time and aggregate world-block paths before reading export DTO state.
- Native and managed persistence probes validate the path policy and the
  managed environment handoff.

### Server world-time persistence wrapper removal

These files now satisfy the current restriction for removing the production
world-time persistence wrapper:

- `octaryn-server/Source/Persistence/WorldTime/WorldTimeStore.cs`
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`
- `tools/validation/Octaryn.WorldTimeProbe/Program.cs`
- `tools/validation/Octaryn.ServerPersistenceProbe/Program.cs`

Current done behavior:

- The managed `WorldTimeStore` file is deleted.
- `SaveExportBundleFile.FromWorldRoot` calls native world-persistence
  `TryReadWorldTimeFile` directly after asking native path policy for
  `world_time.json`.
- Managed validation probes keep local probe-only world-time read/write helpers
  so setup and round-trip checks do not reintroduce a production persistence
  wrapper.
- Managed and native persistence probes validate world-time file read/write,
  save-export bundle import/export, and direct native persistence handoff.

### Server world metadata file wrapper removal

These files now satisfy the current restriction for removing the production
world-metadata persistence wrapper:

- `octaryn-server/Source/Persistence/WorldSave/WorldSaveMetadataFile.cs`
- `octaryn-server/Source/Persistence/WorldTime/WorldTimeFile.cs`
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`
- `tools/validation/Octaryn.ServerPersistenceProbe/Program.cs`
- `tools/validation/Octaryn.WorldTimeProbe/Program.cs`

Current done behavior:

- The managed `WorldSaveMetadataFile` file is deleted.
- The old standalone managed `WorldTimeFile` DTO file is deleted; the save
  export world-time DTO now lives beside the save export bundle DTO that owns
  the gzip JSON shape.
- Managed persistence validation keeps local probe-only world-metadata
  read/write helpers that call native world-persistence directly, so validation
  can inspect the JSON shape without reintroducing a production wrapper.
- Native world-persistence remains the owner for `world_meta.json` and
  `world_time.json` file IO.

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
- In-memory save-export chunk-column override normalization now calls the native
  world-persistence upgrade policy, so managed export/import DTO code no longer
  duplicates legacy local/world coordinate detection or depends on a managed
  `BlockLimits` mirror.
- The managed server persistence probe keeps local probe-only chunk-column file
  helpers so validation can inspect the legacy JSON shape without reintroducing
  production store ownership.
- Probe-only chunk-column file read/write/normalization and edit-conversion
  helpers now live under `tools/validation/PersistenceProbeFiles`; the
  `ChunkColumnOverrideFile` save-export DTO shape now lives beside
  `SaveExportBundleFile`; native persistence ABI types no longer depend on the
  export DTO record.

### Server aggregate world-block file wrapper removal

These files now satisfy the current restriction for removing the production
aggregate world-block file wrapper:

- `octaryn-server/Source/Persistence/WorldBlocks/WorldOverrides/WorldBlockOverrideFile.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldOverrides/WorldBlockOverrideRecord.cs`
- `tools/validation/PersistenceProbeFiles/WorldBlockOverrideProbeFile.cs`

Current done behavior:

- The production managed `WorldBlockOverrideFile` and
  `WorldBlockOverrideRecord` files are deleted.
- Validation probes that need to seed or inspect the legacy JSON aggregate file
  use a linked probe-only helper under `tools/validation/PersistenceProbeFiles`.
- Production aggregate world-block file read/write remains native-owned through
  `NativeWorldPersistenceLibrary` and the server world-persistence library.

### Server terrain generation sampling

These files now satisfy the current restriction for server generated-block
sampling:

- `octaryn-server/Source/World/Generation/TerrainGeneration.cpp`
- `octaryn-server/Source/World/Generation/TerrainGeneration.h`
- `octaryn-server/Source/World/Generation/NativeTerrainGenerationLibrary.cs`
- `octaryn-server/Source/World/Generation/NativeTerrainGenerationTypes.cs`
- `octaryn-server/Source/Modules/ModuleActivator.cs`
- `octaryn-shared/Source/World/Terrain/IWorldGenerationRules.cs`
- `octaryn-shared/Source/World/Terrain/TerrainMaterialRules.cs`
- `octaryn-basegame/Source/Content/Worldgen/WorldGenerationRules.cs`
- `tools/Source/ServerTerrainGenerationProbe/ServerTerrainGenerationProbe.cpp`
- `tools/validation/Octaryn.ServerWorldGenerationProbe/Program.cs`

Current done behavior:

- Native terrain-generation code owns old-architecture column height/material
  planning, floor chunk-local mapping, water/surface/fill selection,
  world-bounds checks, empty-world sampling, and the empty-world white block ID.
- Native terrain-generation code owns generated-override cleanup for both
  basegame terrain rules and the native empty world. Managed activation no
  longer calls a generic managed callback once per saved edit to decide whether
  an override matches generated terrain.
- The managed `TerrainGenerator` and `NativeEmptyWorldGenerator` wrapper
  objects are deleted. Managed activation and validation probes now call the
  native terrain-generation interop surface directly for generated-block
  sampling, empty-world block identity, and generated-override cleanup.

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
- Managed activation marshals stable material block rules from the module
  registration once through `NativeTerrainGenerationLibrary` and no longer
  calls back into managed `PlanTerrainColumn` for generated-block samples.

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
- Native player-simulation code owns process player-input intent read/plan/frame
  composition through one native result, including the `HostFrameSnapshot` ABI
  shape used for server ticks.
- Native player-simulation code owns player-input process stop-reason labels for
  the existing live-log contract.
- Managed `ChunkStreamProcessBridge` keeps existing
  `server_live_player_input_intent` log strings and passes the native frame to
  process tick dispatch.
- The native probe validates successful tick planning, process frame
  composition, unsupported-intent stop planning, and native-owned reason labels.

### Server player session save bookkeeping

These files now satisfy the current restriction for player session state and
save-cadence bookkeeping:

- `octaryn-server/Source/Simulation/Players/PlayerStateLoad.cpp`
- `octaryn-server/Source/Simulation/Players/PlayerSimulation.h`
- `octaryn-server/Source/Simulation/Players/NativePlayerSimulation.cs`
- `octaryn-server/Source/Simulation/Players/NativePlayerSimulationTypes.cs`
- `octaryn-server/Source/Simulation/Players/PlayerController.cs`
- `octaryn-server/Source/Simulation/Players/PlayerState.cs`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStreamProvider.cs`
- `octaryn-server/Source/Modules/ModuleActivator.cs`
- `tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationSaveProbe.cpp`

Current done behavior:

- Native player-simulation code owns the player session struct, current state,
  last saved state, loaded-from-save flag, elapsed save timer, save decisions,
  and successful-save acknowledgement.
- Native player-simulation code now owns player session lifetime behind an
  opaque handle; managed controller code stores only the handle and disposes it
  through the native owner instead of mirroring the session layout in C#.
- Native player-simulation code owns player control-mode live-log label mapping
  so managed controller logging does not duplicate mode-name policy.
- Native player-simulation code owns fly-control-mode identity checks for chunk
  stream metadata, and the managed `PlayerControlMode` enum is deleted.
- Managed `PlayerController` keeps persistence invocation, snapshots, and
  existing live-log text while delegating spawn alignment, tick stepping, and
  save bookkeeping to native session APIs.
- The native probe validates session creation, unchanged elapsed-save tracking,
  changed-state save release, save-state projection, saved-baseline updates,
  and opaque handle creation/state/lifetime failure paths.

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
- Native block-store code owns effective block queries for block-edit lookups:
  stored overrides take precedence, generated blocks are returned when no
  override exists, and missing generated blocks fall back to air.
- Managed `BlockEditService.ApplyCommand` is interop glue over the native
  apply-command ABI.
- The native probe validates command apply success, unknown-block rejection,
  occupied client-interaction rejection, effective query fallback/override
  precedence, and unsupported-block cascade clearing.

### Server block-store managed wrapper surface

These files now satisfy the current restriction for validation-only managed
block-store wrapper removal:

- `octaryn-server/Source/World/Blocks/Store/BlockStore.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreLibrary.cs`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreProbe.cpp`
- `tools/validation/Octaryn.ServerWorldBlocksProbe/ServerWorldBlockStoreValidation.cs`

Current done behavior:

- Managed `BlockStore` no longer exposes native coordinate mapping,
  validity-check, clear-override, try-get, or chunk-column snapshot helpers that
  were only used by managed validation or already covered by native probes.
- Managed `NativeBlockStoreLibrary` no longer resolves those validation-only
  block-store exports for C# callers, and no longer resolves direct
  block-change queue enqueue/drain exports that production C# does not call.
- The native block-store probe remains the owner proof for chunk/local mapping,
  valid-position checks, clear override behavior, try-get behavior, and
  chunk-column snapshot selection, plus direct block-change queue enqueue/drain
  behavior.
- Managed world-block validation keeps integration coverage through the public
  server store/edit/persistence flow instead of reaching into native indexing
  helpers or direct native replication queue helpers through production C#.

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
read/result mapping and process-stage tick/write planning:

- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamWriteTracker.cpp`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.h`
- `octaryn-server/Source/World/Chunks/Streaming/NativeChunkColumnStreamTypes.cs`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreLibrary.cs`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreChunkStreamProbe.cpp`

Current done behavior:

- Native chunk-stream code reads the process chunk-view intent and maps
  missing, retry, partial, unsupported, and failed reads into the existing
  process write-plan reason/handle-result contract.
- Native chunk-stream code owns process write-plan stop-reason labels for the
  existing live-log contract.
- Native chunk-stream code owns live process-stream tick execution selection:
  no-op ticks, default-frame construction, metadata-only host ticks, full ticks,
  and callback failure propagation are dispatched by native owner code.
- Native chunk-stream code owns the combined process-stage plan after intent
  handling: tick decision plus stream write/skip decision are returned in one
  native owner report before managed callback/log glue runs.
- Managed `ChunkStreamProcessBridge` no longer owns chunk-view intent file
  existence checks, read-result mapping, or tick/write decision composition; it
  keeps environment path lookup, live-log text, player/block/world-time glue,
  tick callbacks, and snapshot write orchestration.
- The native probe validates successful process-intent reads plus unsupported
  and missing-intent stop plans, combined process-stage planning, process tick
  dispatch paths, default-frame construction, and callback failure results.

### Server chunk-stream process snapshot output

These files now satisfy the current restriction for process-stream snapshot
write composition:

- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.cpp`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.h`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStreamProvider.cs`
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`
- `octaryn-server/Source/Modules/ModuleActivator.cs`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreProcessSnapshotProbe.cpp`

Current done behavior:

- Native chunk-stream code now owns process-stream snapshot request composition
  from the chunk-view intent and native write plan, including selected write
  center/radius, previous-window reuse, metadata-only flag, and tracker
  note-written behavior after a successful file write.
- Managed `ChunkStreamProcessBridge` no longer computes previous-window write
  arguments or marks the native write tracker directly; it passes the process
  intent, native write plan, and server-owned world/player snapshots to the
  chunk-stream owner.
- The old managed direct snapshot-file wrapper path was removed from
  `ChunkColumnStreamProvider`/`ModuleActivator`; managed code no longer exposes
  a C# method that manually mirrors the native snapshot request fields.
- The native probe validates process snapshot writing and proves a repeated
  metadata-only request skips after the native process-snapshot writer marks
  the plan as written.

### Server chunk-stream process bridge

`octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs` is
demoted from owner-system blocker to glue/watchlist for the current restriction.

Current source evidence:

- Native host policy owns live process-stream path lookup, metadata-only flag
  parsing, and request gating before the bridge starts a process iteration.
- Native chunk-stream code owns chunk-view intent reads, process write plans,
  process tick/default-frame dispatch decisions, write/skip decisions, snapshot
  request composition, and write-tracker note-written behavior.
- Native player simulation owns player-input intent read/plan/frame composition.
- Native block-store code owns block-interaction intent reads, command summaries,
  read-result planning, duplicate-frame planning, and stop-reason labels.
- Native world-time code owns speed-intent reads, process planning, and
  stop-reason labels.
- Managed `ChunkStreamProcessBridge` now sequences native owner calls, invokes
  module callbacks, marshals paths/strings, submits already-packed commands,
  snapshots world/player DTO state for the native snapshot writer, and preserves
  existing live-log text.
- The old architecture had managed-frame snapshot ABI construction but no direct
  process-file stream bridge matching this newer validation path; current native
  owner code is therefore the source of truth for this bridge boundary.

### Server client block command queue submit/drain

These files now satisfy the current restriction for client block command queue
submit reporting and draining:

- `octaryn-server/Source/World/Blocks/Store/BlockCommandQueue.cpp`
- `octaryn-server/Source/World/Blocks/Store/BlockCommandQueueApi.cpp`
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
- Native block-store code owns client block-command submit reason labels
  (`accepted`, `capacity`, `rejected_command`, `native_submit`); managed
  activation keeps live-log output only.
- Native block-store code drains queued client block commands and applies them
  through the same native set-block command policy used by direct server
  commands.
- Native block-store code returns the queued-command drain report shape used by
  managed activation logs: applied count and pending-after count.
- Native block-store code enqueues changed block edits into the native
  replication-change queue during direct command apply and queued-command drain;
  managed command glue no longer loops over changed edits to propagate
  replication changes.
- Managed `ClientBlockCommandQueue` keeps allocation, submit, pending-count,
  dispose, and live-log glue only; it no longer owns drain-time command
  application callbacks or post-drain pending-count lookup.
- Managed `BlockCommandSink` still owns live-log output and persistence dirty
  notification after native apply/enqueue results are returned.
- The native probe validates submit rejection, drain-apply success, drain-apply
  rejection, native change-queue enqueueing, store mutation, callback result
  counts, and queue clearing.
- Native queue behavior and exported C ABI wrappers are split into separate
  focused source files so future command-queue work does not breach the
  500-line source cap.

### Server block command sink glue

`octaryn-server/Source/World/Blocks/Commands/Queue/BlockCommandSink.cs` is
demoted from owner-system blocker to glue/watchlist for the current restriction.

Current source evidence:

- Old-architecture command source in `old-architecture/source/api` only defines
  the native command ABI shape and set-block constructor; current block-command
  ownership is defined by the native server block-store code.
- Native block-store code owns host-command current ABI validation, supported
  set-block command recognition, client-interaction validation, edit labels,
  submit result/reason planning, queue drain/apply, and replication-change
  enqueueing.
- Native block-edit service code owns set-block preflight, effective-block
  lookup, block-known/support policy application, changed-edit generation, and
  store mutation.
- Managed `BlockCommandSink` keeps the shared `IHostCommandSink` adapter,
  fallback forwarding for non-block commands, existing `server_live_block_command`
  log text, and persistence dirty callback after native apply/drain results.
- Native and managed probes cover direct block-command application,
  current-command rejection, client-interaction rejection, queued drain/apply,
  native change-queue enqueueing, and managed host-command sink integration.

### Server snapshot drain reporting

These files now satisfy the current restriction for server snapshot drain
result reporting:

- `octaryn-server/Source/World/Blocks/Store/BlockChangeQueue.cpp`
- `octaryn-server/Source/World/Blocks/Store/BlockChangeQueue.h`
- `octaryn-server/Source/World/Blocks/Store/BlockChangeQueue.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreLibrary.cs`
- `octaryn-server/Source/World/Blocks/Store/NativeBlockStoreTypes.cs`
- `octaryn-server/Source/Modules/ModuleActivator.cs`
- `tools/Source/ServerBlockStoreProbe/ServerBlockStoreProbe.cpp`

Current done behavior:

- Native block-store code owns the snapshot drain report shape returned to
  managed activation: result, requested capacity, pending-before count, and
  written count.
- Managed `ModuleActivator.DrainServerSnapshots` calls the native report API and
  keeps only the existing `server_live_snapshot_drain` log text.
- The previous native drain ABI remains covered while the native probe validates
  the report API and header tick/count updates.

### Server module activation runtime composition

`octaryn-server/Source/Modules/ModuleActivator.cs` is demoted from
owner-system blocker to glue/watchlist for the current restriction.

Current source evidence:

- Module validation, bundled manifest checks, instance creation, and module tick
  callback routing are allowed module-activation glue.
- Native authority-tick code owns client-command drain, player simulation, and
  world-time advancement ordering through `octaryn_native_jobs`; managed
  activation only supplies callbacks and logs the existing tick summary.
- Native block-store code owns block edit application, client-command queue
  submit/drain reports, snapshot-drain reports, block-change queue mutation, and
  chunk-stream request availability; managed activation keeps interop, live-log
  text, and dirty notifications.
- Native terrain-generation code owns generated block sampling, material-rule
  planning, empty-world identity, and generated-override cleanup; managed
  activation only selects the module-provided rule source and calls the native
  cleanup/sampling APIs.
- Native persistence code owns root/path selection, load-source decisions,
  dirty/save-needed policy, and save backends; managed activation invokes
  load/initialize/save checkpoints around native-backed stores.
- `ModuleActivator.cs` still deserves watchlist scrutiny because it composes the
  server runtime object graph, but the reviewed responsibilities are
  orchestration glue around focused native owner systems rather than standalone
  C# engine-system implementations.

### Server save-export gzip file lifecycle

These files now satisfy the current restriction for save-export gzip file
lifecycle ownership:

- `octaryn-server/Source/Persistence/WorldBlocks/SaveExportBundle.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistenceGzip.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeGzipPersistence.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeSaveExportBundlePersistence.cs`
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`
- `tools/Source/ServerWorldPersistenceProbe/GzipPersistenceProbe.cpp`
- `tools/Source/ServerWorldPersistenceProbe/SaveExportBundleProbe.cpp`
- `tools/validation/Octaryn.ServerPersistenceProbe/Program.cs`

Current done behavior:

- Native world-persistence code creates parent directories for gzip exports,
  writes the compressed payload through a `.tmp` path, and replaces existing
  export files.
- Native world-persistence code serializes/deserializes the save-export bundle
  JSON shape inside the gzip payload, preserves unsupported bundle versions for
  load-time rejection, rejects unsupported nested player save versions before
  flattening player state through the native ABI, and exposes count/fill/write
  APIs for managed glue.
- Managed `SaveExportBundleFile.SaveGzip` and `TryLoadGzip` now map DTOs to and
  from native arrays instead of owning gzip payload JSON serialization.
- The server persistence probe validates gzip export write, temp cleanup,
  existing-file overwrite, reload, and import behavior.
- The native world-persistence probe validates bundle gzip count/fill/write,
  nested player/world-time/chunk data, legacy chunk version preservation, and
  unsupported bundle/player-version rejection through the native codec API.

### Server save-export bundle import writes

These files now satisfy the current restriction for save-export bundle import
write orchestration:

- `octaryn-server/Source/Persistence/WorldBlocks/WorldSaveImport.cpp`
- `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.h`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeSaveExportBundlePersistence.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeWorldPersistenceLibrary.cs`
- `octaryn-server/Source/Persistence/WorldBlocks/NativeWorldPersistenceTypes.cs`
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`
- `tools/Source/ServerWorldPersistenceProbe/WorldSaveImportProbe.cpp`

Current done behavior:

- Native world-persistence code owns import writes for bundle world-time,
  player-directory entries, and chunk override edits under a world root.
- Native import code normalizes legacy chunk-column DTO coordinates before
  saving aggregate and sidecar world-block override files.
- Native import code asks world-persistence path policy for root-relative
  `world_time.json` and `world_blocks.json` paths instead of duplicating those
  child names locally.
- Native import code validates the save-export bundle version, world-time
  payload version, nested player payload versions, and chunk DTO validity before
  any import write.
- Managed `SaveExportBundleFile.WriteToWorldRoot` keeps DTO-to-native mapping,
  then calls one native import entry point instead of looping over persistence
  backends or owning import version rejection itself.
- The native world-persistence probe validates world-time/player writes,
  chunk-column normalization, aggregate world-block output, chunk-column counts,
  unsupported import version rejection, unsupported chunk rejection, and
  no-partial-write behavior through the native import API.

### Server player controller glue

`octaryn-server/Source/Simulation/Players/PlayerController.cs` is demoted from
owner-system blocker to glue/watchlist for the current restriction.

Current source evidence:

- Native player-simulation code owns default player state, saved-state finite
  checks, pitch/yaw normalization, spawn alignment, movement/collision, idle
  updates, control-mode labels, session state, save cadence, save-state
  projection, saved-baseline acknowledgement, and opaque session lifetime.
- Native world-persistence code owns the player-directory path policy, player
  file load, player file write, filename shaping, and JSON persistence backend.
- Managed `PlayerController` keeps a native session handle, invokes native
  player simulation and native-backed player persistence, returns the existing
  player snapshot DTO, disposes the native handle, and preserves current
  `server_live_player_*` log text.
- Old architecture player movement/spawn/save policy maps to the current native
  `PlayerSimulation` and `PlayerStateLoad` files, while old player persistence
  maps to the native world-persistence player directory path.

## Not Done

## Finish-Blocker Rule

The loop is now in blocker-sized finish mode. Future passes must remove,
demote, or materially shrink one blocker below, or produce final runtime proof.
Do not spend a pass only moving labels, reason strings, flags, one-line helper
predicates, thin ABI wrappers, or probe bookkeeping unless that work is inside a
larger blocker-closing change.

Required blocker order:

- No ordered finish blockers remain for the current blocker-sized pass list.

### Whole finish plan

The finish plan is not complete. Current active C# file counts:

- `octaryn-client`: 7
- `octaryn-server`: 58
- `octaryn-shared`: 79
- `octaryn-basegame`: 13

The client count is acceptable for current glue/API restrictions. The server
count is not.

### Server C# glue/watchlist

These files still deserve scrutiny during related slices, but current source
review does not show them owning the engine behavior previously listed here:

- `octaryn-server/Source/World/Blocks/Commands/Queue/BlockCommandSink.cs`:
  shared host-command sink adapter, non-block fallback forwarding, existing
  live-log text, and persistence dirty callback around native command
  validation/apply/drain/enqueue owner APIs.
- `octaryn-server/Source/Simulation/Players/PlayerController.cs`: native player
  session handle lifetime, native-backed player persistence invocation, player
  snapshot DTO handoff, and existing live-log text around native simulation and
  native persistence owner APIs.
- `octaryn-server/Source/Host/Host.cs`: startup activation, ready/shutdown
  console signals, and callback glue around native host policy and native
  live-stream loop execution.
- `octaryn-server/Source/Modules/ModuleActivator.cs`: module activation,
  runtime owner composition, native-backed persistence checkpoints,
  command-write module tick callback routing, snapshot-drain live-log text, and
  dirty notification glue around native owner APIs.
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`:
  process-stream orchestration glue around native host policy, native
  chunk-stream read/plan/write ownership, native player/block/world-time intent
  planning, module callbacks, world/player DTO snapshots, and existing live-log
  text.
- `octaryn-server/Source/World/Blocks/Store/BlockStore.cs`: native block-store
  handle/lifetime plus production block DTO handoff for block count, get, set,
  snapshot, and load. Validation-only coordinate, clear, try-get, and
  chunk-column snapshot wrappers were removed; native owner probes cover those
  rules directly.
- `octaryn-server/Source/World/Blocks/Store/BlockChangeQueue.cs`: native
  change-queue handle/lifetime plus pending-count and snapshot-drain report
  handoff. Direct enqueue/drain wrappers were removed from production C#;
  native owner probes cover those queue rules directly.
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`:
  save-export DTO-to-native mapping and the production DTO surface. Gzip file
  lifecycle, bundle JSON codec, player directory scan/load filtering, chunk
  export loading/planning, and world-root import writes are native.

### Shared and basegame boundary

- `octaryn-shared` must remain API/contracts/validation/sandbox policy only.
  Any storage, scheduler, persistence, stream, simulation, render, or host
  implementation type is not done.
- `octaryn-basegame` may stay C# only as bundled gameplay/content module logic
  using shared contracts. It must not gain client rendering, server
  persistence, transport, native bridge internals, raw threading, filesystem, or
  host service ownership.

### Runtime/profiling proof gaps

The current direct runtime log proves the radius-32 batching, retained
direct-indirect draw, profiling, metadata-only stream, and no seed-terrain
persistence requirements for the final blocker-sized proof. Future visual or
lag-focused work still needs fresh screenshot/RenderDoc/Tracy evidence before
claiming visual correctness or FPS regression closure.

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
