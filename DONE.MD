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

### Server authority tick ordering

These files now satisfy the current restriction for the player/world-time
authority tick ordering seam:

- `octaryn-server/Source/Tick/AuthorityTick.cpp`
- `octaryn-server/Source/Tick/AuthorityTick.h`
- `octaryn-server/Source/Tick/AuthorityTickRunner.cs`
- `octaryn-server/Source/Tick/NativeAuthorityTickLibrary.cs`
- `tools/Source/ServerAuthorityTickProbe/ServerAuthorityTickProbe.cpp`

Current done behavior:

- The server-owned native authority tick plan schedules player simulation before
  world-time advancement through `octaryn_native_jobs`.
- Managed `AuthorityTickRunner` is interop glue that supplies callbacks and
  validates the native schedule report.
- The native probe validates ordering, dependency waves, worker routing, and
  callback failure short-circuiting.

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
- Managed `WorldBlockPersistence` keeps only environment path selection,
  block-store snapshot/load handoff, and native interop conversion.
- Managed `SaveExportBundleFile` imports chunk DTOs by validating bundle
  versions, converting DTO blocks to native edits, and calling the same native
  aggregate/sidecar save coordinator instead of writing `world_blocks.json`
  directly.
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

## Not Done

### Whole finish plan

The finish plan is not complete. Current active C# file counts:

- `octaryn-client`: 7
- `octaryn-server`: 66
- `octaryn-shared`: 78
- `octaryn-basegame`: 13

The client count is acceptable for current glue/API restrictions. The server
count is not.

### Server C# owner-system blockers

These server files and areas still own too much engine/runtime behavior and must
continue moving into focused C++ owner code:

- `octaryn-server/Source/Modules/ModuleActivator.cs`: runtime composition,
  persistence save calls, command drain, and snapshot drain orchestration.
- `octaryn-server/Source/Host/Host.cs`: live process loop and sleep-driven
  stream handling.
- `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`:
  process-stream request handling, intent file/env orchestration, ticking,
  snapshotting, and stream output decisions.
- `octaryn-server/Source/Persistence/WorldBlocks/ChunkColumns/ChunkColumnOverrideStore.cs`:
  chunk override store path/interop orchestration.
- `octaryn-server/Source/Persistence/WorldSave/SaveExportBundleFile.cs`: save
  export/import DTO, gzip serialization, and chunk bundle orchestration; player
  directory scan/load filtering and world-block import writes are native.
- `octaryn-server/Source/Simulation/Players/PlayerController.cs`: player state
  ownership, save cadence calls, persistence invocation, and logging around
  native stepping.
- `octaryn-server/Source/World/Blocks/Commands/Queue/ClientBlockCommandQueue.cs`:
  managed submit/drain command queue callbacks.
- `octaryn-server/Source/World/Blocks/Commands/Queue/BlockCommandSink.cs`:
  managed block command apply/enqueue orchestration.
- `octaryn-server/Source/World/Generation/TerrainGenerator.cs`: managed
  generation-rule callback path around native terrain generation.

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
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_authority_tick_native_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_owner_boundaries`
- server native-owner probes matching the touched server slice.
