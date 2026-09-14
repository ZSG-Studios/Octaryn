# Restored engine feature parity audit

Current backend direction (2026-09-13): the active open world has been ported to
standalone `shader-slang/slang-rhi`, with Vulkan selected through its API.
See [pipeline-parity.md](pipeline-parity.md) for the current integrated build,
GPU captures and remaining limits. The sky/G-buffer/HDR/forward graph, local
skinned player, fluids, clouds, selection and RmlUi are now verified on this
backend. GFX-specific findings and the initial archive inventory below are
historical snapshots, not current absence claims.

Current fluid gameplay status: the native evaluator, bounded scheduler, module
configuration and authoritative apply/save/publication paths are now integrated.
Production-owner tests cover scheduled water/lava falling and persistence, while
native fixtures cover the broader original rules and backpressure. The rebuilt
package awaits installation after the running game exits; combined live fluid
behavior and sustained large-region workloads remain unqualified. See
[fluid-simulation-recovery.md](fluid-simulation-recovery.md).

Natural vegetation gap confirmed in the active generation call graph:
WorldGenerationRules.AddFeatureBlocks has managed rule tests, but no active
server/client generation caller. Native TerrainDensity and client GenerateColumn
currently reconstruct base terrain without that decoration stage. Existing
terrain reconstruction parity is therefore not evidence of integrated natural
trees, bushes or flowers. Restore this through matching authoritative/client
generation semantics and generation-version/save qualification; do not inject
decorations only into the renderer or mutate the saved baseline silently.

## Original presentation source recovered

The development archive omitted the older renderer reference. The separate
read-only `ref/upstream-octaryn` checkout now contains it at commit
`3557cbfdc803ec034122bb55070b62b3b43b5588`. See
[presentation-restoration.md](presentation-restoration.md) for the actual original
pass/material/UI/player mapping. The initial minimal renderer has since been
extended with the restored presentation owners and actual GPU verification.

This newer evidence supersedes conclusions below that infer absence solely from
the tarball: the original texture atlas/animation, scene passes and UI draw code
exist in the recovered source. Humanoid rendering was absent at the original
audit; the retained asset now has an importer, authored animation sampling and
Slang RHI skinning/drawing. Remote-avatar replication remains unimplemented.

Audited 2026-09-13. This is a source and content audit, not a claim that the
restored game has passed a native build or interactive runtime qualification.
Read the active sections of AGENTS.md and REQUESTS.md before acting on it.

## Reference and preservation

The comparison source is the preserved extraction at:

`C:\Users\Rose-X\Documents\Octaryn-Backups\2026-09-13-before-old-engine\workspace\ref\octaryn-workspace-dev`

The recent networking implementation is separately preserved at:

`C:\Users\Rose-X\Documents\Octaryn-Backups\2026-09-13-before-old-engine\workspace`

SHA-256 comparison of every reference file under the four owner roots found
489 files present and identical at the audit snapshot: client 187, server 122,
shared 109, basegame 71; zero missing files and zero changed files. This includes
owner project files, source, data and assets. Concurrent build repairs after
this snapshot are intentional changes and must be qualified separately.
The backup was only read. Restoring those files preserves the archive's own
unfinished integrations as well as its implemented systems.

## Archived baseline inventory

The following table records the initial archive audit. Later integration results
below and `presentation-restoration.md` supersede its unconnected-runtime rows.

All paths in this table are relative to the active repository root. Presence
means inspected implementation exists; it does not mean the live game calls it.

| Feature | Existing implementation and contracts | Status and remaining proof |
| --- | --- | --- |
| Terrain generation | `octaryn-server/Source/World/Generation/TerrainGeneration.cpp`; `octaryn-basegame/Source/Content/Worldgen/WorldGenerationRules.cs`; `octaryn-basegame/Data/Rules/octaryn.basegame.rule.terrain_generation.json` | Native noise/terrain generation and basegame material, water, tree and plant rules exist. Verify native/module parity at negative coordinates, chunk boundaries and generated height limits; visual terrain parity remains unverified. |
| Authoritative block edits | `octaryn-server/Source/World/Blocks/Store/BlockStore.cpp`, `BlockEditService.cpp`; `octaryn-server/Source/World/Blocks/Commands/Queue/BlockCommandSink.cs`; `octaryn-basegame/Source/Gameplay/Interaction/BlockAuthorityRules.cs` | Store, edit service, command validation and basegame rules exist. Prove break/place, reach rejection, support changes, generated-block lookup and repeated commands through the live client/server path. |
| Chunk streaming | `octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.cpp`, `ChunkStreamBinarySnapshot.cpp`, `ChunkStreamWriteTracker.cpp`, `ChunkStreamProcessBridge.cs`; `octaryn-client/Source/Rendering/VoxelWorld/ColumnStreaming.cpp` | Window plans, binary sidecar output, unchanged-window gating and bounded column state exist. Client rendering reads a sidecar or a fixture. Runtime must establish real server provenance, bounded work, retention and full radius visibility. |
| Player movement | `octaryn-server/Source/Simulation/Players/PlayerController.cs`, `PlayerSimulation.cpp`, `PlayerJoltMovement.cpp`, `PlayerJoltWorld.cpp`; `octaryn-client/Source/Input/PlayerControl/PlayerControlInput.cpp`; `octaryn-client/Source/Player/FlyController/FlyPlayerController.cpp` | Walk/sprint/jump/fly, spawn alignment, Jolt collision and native session state exist. Client controls and server state are not established as connected to a continuous interactive app. |
| Player persistence | `octaryn-server/Source/Persistence/WorldBlocks/PlayerPersistence.cpp`, `PlayerDirectory.cpp`; `octaryn-server/Source/Simulation/Players/PlayerStateLoad.cpp` | Native save/load, directory policy and state normalization exist. Require movement-save-restart-restore proof; do not infer it from the player-state stream JSON. |
| World saves and edits | `octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.cpp`, `ChunkOverridePersistence.cpp`, `ChunkOverrideDirectory.cpp`, `WorldMetadataPersistence.cpp`, `WorldSaveImport.cpp`, `SaveExportBundle.cpp` | Override/sidecar loading, dirty tracking, pruning, metadata and import/export exist. Validate edited air, unchanged-world no-write behavior and export/import round trips. Generated seed chunks must remain absent from persisted block records. |
| World clock and authority ordering | `octaryn-server/Source/World/Time/WorldTimeClock.cs`; `octaryn-server/Source/Tick/AuthorityTick.cpp`, `AuthorityTickRunner.cs`; `octaryn-server/Source/Persistence/WorldBlocks/WorldTimePersistence.cpp` | Native clock and ordered command-drain, player, world-time jobs exist. This is not the LES fixed-tick loop from the recent backup. Live process timing uses elapsed host time with a clamp. |
| Local process communication | `octaryn-server/Source/World/Chunks/Streaming/ChunkStreamProcessBridge.cs`; `octaryn-server/Source/Host/Host.cs`; `octaryn-client/Source/App/RuntimeFiles/JsonContracts.h` | Server reads intent files and writes chunk/player state files. Host has a background process-stream mode. Contracts and file exchange are not internet multiplayer. |
| Network transport and remote interpolation | `octaryn-shared/Source/Networking/Messages/NetworkMessage.cs`, `Snapshots/ServerSnapshot.cs`, `Commands/ClientCommand.cs`; `octaryn-client/Source/HostBridge/Exports/HostExports.cs` | Shared contracts exist; no LES/LiteNetLib or socket transport implementation was found in the active owner sources. Snapshot apply/drain stubs were repaired to queue existing block edits, detailed below; no renderer consumer or equivalent of the backup's source-time render buffer is integrated. |
| UI and display controls | `octaryn-client/Source/Ui/DisplayMenu/DisplayMenu.cpp`; `octaryn-client/Source/Ui/RuntimeControls/Menu/Menu.cpp`, `Events/Events.cpp`, `Entrypoints/RuntimeControls.cpp`; `octaryn-client/Source/Display/DisplayCatalog/DisplayCatalog.cpp` | SDL display/menu/event logic exists. `Entrypoints/Stub.cpp` supplies no-op behavior when `RUNTIME_CONTROLS_USE_SDL3` is absent. Actual UI drawing, event routing and setting application need interactive validation. |
| Assets and basegame content | `octaryn-basegame/Assets/Atlases/basegame-color.png`, `basegame-normal.png`, `basegame-specular.png`, `basegame-animation.png`, `basegame-animation.txt`; `octaryn-client/Assets/Player/octaryn_player_v1.gltf`; `octaryn-basegame/Source/Content/Blocks/BlockCatalog.cs` | Assets, material mapping, animation metadata and player model are preserved. Content validators passed below. Runtime atlas sampling, animation and player-model rendering are separate checks. |
| Module API and loading | `octaryn-client/Source/Host/Modules/GameModuleActivator.cs`; `octaryn-server/Source/Modules/ModuleActivator.cs`; `octaryn-basegame/Source/Module/ModuleRegistration.cs`; `octaryn-basegame/Data/Module/octaryn.basegame.module.json` | Managed modules and native host bridges exist. Prove packaged module loading and ABI calls with the actual Windows bundle; do not count missing-hostfxr placeholders. |
| Profiling | `octaryn-client/Source/Diagnostics/FrameMetrics/FrameMetrics.cpp`; `octaryn-client/Source/Diagnostics/FrameProfile/FrameProfile.h`; `octaryn-shared/Source/Diagnostics/NativeProfiling/octaryn_native_profile.cpp` | Metrics infrastructure exists. Fresh frame/stream samples must come from the real running world. The backup's drone traces do not measure this engine. |

## Confirmed integration gaps in the archived baseline

1. `octaryn-client/Source/App/FrameLoop/FrameLoop.h` sets
   `VoxelRuntimeFrames = 3`. Its `.cpp` performs those raster frames and returns
   a readback result. It has no continuous SDL event/input/menu loop.
   `App/SlangRhiBootstrap/SlangRhiBootstrap.cpp` calls this finite loop and a
   swapchain probe. A successful executable run here is renderer qualification,
   not a playable game session.
2. At restoration, `HostBridge/Exports/HostExports.cs::ApplyServerSnapshot`
   validated only the header and the drain always returned zero changes. The
   focused repair below now transfers supported block edits through the ABI.
   A renderer consumer and transport are still missing; passing bridge probes
   must not be described as displayed replication.
3. `server_player_state_stream_file` is declared in `App/RuntimeFiles/JsonContracts.h`.
   No active client reader or use of that type was found. The server produces
   player JSON, but that alone does not connect authoritative motion to a camera
   or rendered player.
4. No active socket transport or recent networking adapter exists under the
   restored client/server/shared source owners. Do not advertise multiplayer
   readiness based on shared message definitions or local file exchange.
5. The UI has both implemented SDL branches and conditional no-op stubs.
   Dependency presence and source inventory do not establish which branch a
   built application uses, or whether its frame loop routes events there.

These gaps are present in the preserved old reference too. They are not evidence
that restoration deleted an otherwise working interactive implementation.

## Networking recovery decision

No simulation timing source was changed during this audit. The recent fix
targets LES stepping BEPU once per simulation tick and rendering replicated
drone snapshots; the restored owner has different lifetime, timing and transport
boundaries. A copy would not repair an independently demonstrated matching bug.

In the restored process bridge, `WithServerElapsedDelta` measures monotonic host
elapsed time and clamps to 1 ms..250 ms. Native player integration subdivides up
to 250 ms into steps no greater than 50 ms. Those policies need measurement under
stalls and rapid polling; their existence alone is not proof of speed error.
The player state stream carries a client-derived frame index, position and
velocity, but no authoritative absolute simulation timestamp.

When the actual player/snapshot consumer is restored, adapt the backup's
source-time invariant, duplicate rejection, bounded history, outage hold/refill
and disconnect reset. Introduce a coherent source tick/time and update both ends
of the contract together. Keep buffer duration configurable; do not copy the
demo's 500 ms default or maximum-body-timestamp shortcut without validation.
See [networking-recovery.md](networking-recovery.md) for exact preserved sources
and the historical observer-impairment mismatch.

The active Jolt movement path builds a local `PhysicsSystem`, adds nearby block
colliders and uses `CharacterVirtual::ExtendedUpdate` for each movement call.
It is not a BEPU rigid-body shard simulation. Jolt's official
[CharacterVirtual API](https://jrouwe.github.io/JoltPhysicsDocs/5.3.0/class_character_virtual.html)
documents that character update operation. Preserve the current character's
collision, ground and coordinate behavior while adapting timing; BEPU handles,
solver callbacks, body pooling and its Z-up floor test do not transfer directly.

## Recovery order and acceptance evidence

1. Establish real Windows owner bundles and renderer capability, recording
   compiler, native dependency and hostfxr failures separately. A missing backend
   or skipped target must remain a failure. Build work is tracked independently.
2. Restore a continuous client event/render loop around the existing owners.
   Demonstrate camera input, display/menu interaction, shutdown and actual
   server-derived world streaming. Preserve bounded per-frame work.
3. Reconnect client input and authoritative player state before changing motion
   smoothing. Show walk, sprint, jump, fly, contact, spawn and save/restart in a
   real session. Profile the current per-step Jolt world construction before
   replacing its lifecycle.
4. Connect break/place to the existing authority, changes and persistence path.
   Demonstrate visible edits and edited-air survival after restart, then verify
   no generated terrain is written as block overrides.
5. Scope actual transport and snapshot consumption explicitly. Recover recent
   clock/interpolation/queue/lifecycle behavior against that protocol; test
   stale/duplicate data, jitter, stalls, refill, reconnect and reused identities.
   Multiplayer acceptance requires separate clients communicating over the
   selected transport, not a local process-file probe.
6. Measure radius-32 completeness and elapsed streaming, batch/job/upload costs,
   retained resources, indirect draw behavior and real frame delivery. Verify
   larger radii only after that baseline. Linux/macOS need native qualification;
   package production is not runtime proof.

## Validation performed in this audit

- SHA-256 source/content comparison: 489 reference owner files, zero missing or
  changed at the recorded snapshot.
- `tools/validation/validate_basegame_worldgen_content.py`: passed against the
  active block catalog, biome, feature and terrain-generation JSON files.
- `tools/validation/validate_basegame_block_catalog.py`: passed with the active
  generated `BlockCatalog.cs`, color/normal/specular atlas files and animation
  atlas/manifest arguments.
- Read the active instructions, restoration/recovery documents and production
  implementations listed above. No native aggregate build, app launch or physics
  probe was run by the inventory audit. Subsequent focused managed validation is
  recorded below. No preserved backup changed.

Existing focused probes to use after dependency/build readiness include
`tools/validation/Octaryn.ServerWorldGenerationProbe`,
`Octaryn.ServerWorldBlocksProbe`, `Octaryn.ServerPersistenceProbe`,
`Octaryn.WorldTimeProbe`, `Octaryn.BasegamePlayerProbe`, and
`Octaryn.BasegameInteractionProbe`, plus native
`tools/Source/ServerPlayerSimulationProbe` and
`tools/Source/ClientVoxelWorldFrameLoopProbe`. Passing those checks supplements
the required live-path evidence; it does not replace it.

## Focused block snapshot ABI recovery

`octaryn-client/Source/WorldPresentation/BlockUpdates/BlockUpdateQueue.cs` now
copies the server's existing kind-1 `BlockReplicationChange` records into a
client-owned FIFO. `HostBridge/Exports/HostExports.cs` applies complete batches,
drains up to caller capacity and clears state on shutdown/reinitialization.
The schema and payload are unchanged. No new transport, world authority,
renderer integration, player pose schema or interpolation was introduced.

The queue holds at most 4096 records. Error `-2` rejects malformed or unsupported
changes/entity lists; `-3` rejects a batch that cannot fit. Rejected batches
enqueue nothing, and backpressure never silently drops edits. A future caller
must retain and retry a rejected batch after draining. Snapshots with the same
tick are not discarded: the server may drain separate batches from one tick.
Pointers remain trusted in-process ABI buffers whose validity/lifetime is the
caller's responsibility; header validation cannot prove arbitrary memory safe.

The existing native client launch probe and its log validator now require a
copied edit and rejection of an unsupported change kind. A dedicated
`--client-presentation-only` mode in the existing
`Octaryn.OwnerModuleValidationProbe` calls the real unmanaged managed exports
with modules disabled; it requires no native Jolt/renderer build. It covers
copy ownership, signed coordinates, edited air, partial/zero drains, atomic
invalid-batch rejection, backpressure, reinitialization and shutdown.

Focused verification completed:

```powershell
dotnet build tools/validation/Octaryn.OwnerModuleValidationProbe/Octaryn.OwnerModuleValidationProbe.csproj --configuration Release -p:OctarynBuildPresetName=feature-parity-windows -p:OctarynPythonExecutable=python.exe --nologo
dotnet build/feature-parity-windows/tools/Octaryn.OwnerModuleValidationProbe/managed/Octaryn.OwnerModuleValidationProbe.dll --client-presentation-only
```

Managed Release build passed with zero warnings/errors, and the direct ABI probe
passed all cases above. Native launch-probe expectations were updated but the
native launch probe was not run by this slice. This validates handoff into the
presentation queue, not visible block updates or networking end to end.

## Radius-32 evidence correction

The rebuilt native world frame-loop probe passes a server-authored stream with
4225 advertised columns, but retains only 4 chunks and draws 3 sections. The old
radius32_visible flag was computed solely from live_columns >= 4225. It did not
prove visible terrain or GPU residency across that radius. It is now named
radius32_stream_available in code, logs, and matching metadata checks.

This is a metadata/retained-session regression check, not full radius-32 rendering
qualification. Actual complete residency, visible geometry, streaming over a
continuous moving camera, and performance remain integration work. Do not reuse
historical radius32_visible=1 logs as proof of those features.

## Shared terrain and authoritative snapshot ingestion

`octaryn-basegame/Source/Gameplay/Terrain/TerrainColumn.h` now contains the pure
terrain noise, height and material classification formerly embedded in server
`TerrainGeneration.cpp`. The server calls the shared implementation unchanged.
Client `WorldPresentation/WorldStream` reconstructs full 32×512×32 columns from
these same rules, then applies server override records, including explicit air.
The snapshot provider now reports the generator's actual fixed seed, 1337.

The client consumes the existing OCSTRM01 binary sidecar. Its reader bounds file
size/counts, checks column origins and edit coordinates, and only replaces a
valid snapshot after complete validation. A worker thread performs file reads
and column generation; callers request a moving window and poll completed
columns. The ready queue holds at most two columns, and radius is capped at four.
This slice creates voxel data only; GPU meshing remains the renderer's job.

The focused `octaryn_client_world_stream_probe` was directly compiled and passed
against the existing server terrain DLL before that DLL's extraction rebuild:
6,534 block comparisons across six signed/far column locations, full vertical
extent, air/top-boundary edits, malformed seed/truncation rejection preserving
the previous snapshot, asynchronous delivery and old-window retirement.
The probe is an explicit standalone CMake target, outside the default aggregate.

This path currently assumes normal basegame generation and its catalog IDs,
with complete override snapshots from the supervised local server. The binary
format does not negotiate arbitrary module terrain rules or catalog revisions.
Interactive visible residency, editing, save/restart behavior, network transport,
larger radii and performance require separate end-to-end evidence.

## First-person interaction recovery

The recovered upstream reference at commit
`3557cbfdc803ec034122bb55070b62b3b43b5588` supplies original targeting/selection
behavior in `references/old-architecture/source/app/player/player.cpp` and
`blocks.cpp`. Client `WorldPresentation/Interaction/BlockInteraction` now loads
the existing basegame catalog, traces through worker-retained authoritative
columns, picks/cycles selectable blocks, and creates break/adjacent-place intents.
Unknown columns stop a query. Its ten-block target preview retains the original
reach, while actionability respects the restored server's six-block center-distance
limit. The client never applies those intents to terrain.

`LocalSession::submit_block_edit` queues at most 64 pending intents and publishes
one existing-format interaction file at a time. File consumption acknowledges
handling, not successful application; server validation and replication decide
the result. A pending file is never overwritten or automatically replayed, and
two/ten-second wait/timeout states are exposed. The server now clears terminal
rejections as well as accepted commands, retries capacity failures, and retries
cleanup of already consumed frames. Live local commands use the server player's
position for the existing reach check. Collision/support/catalog checks remain
in existing server owners. Input camera diagnostics now carry the presented
authoritative pose instead of zeros.

Direct `ClientInteractionProbe` passed catalog selection, negative-coordinate
queries, ray hit/adjacent placement, preview/action reach separation, unknown
terrain and no-local-mutation checks. Direct LocalSession native compilation and
the managed server Release build passed; the server build had zero warnings or
errors. The isolated `LocalInteractionSessionProbe` then exercised the real
LocalSession/BlockInteraction/WorldStream path with a copied server bundle:
break `(0,33,-1)`, reject remote `(10000,-1,0)` despite a forged nearby camera,
break `(0,33,-2)` after rejection, place dirt at `(0,33,-2)`, then remove it.
Both air overrides streamed back and survived graceful stop/restart. The save
contains exactly those two air records and no remote edit. Evidence is under
`work/interaction-session/world-4` and `work/interaction-session/logs-4`; the
reusable harness is `tools/Source/ClientInteractionProbe/LocalInteractionSessionProbe.cpp`.
This used a fresh isolated world and did not modify production saves. Mouse/HUD
wiring and visible selection still require interactive validation.

The live player transport also carries authoritative `worldTimeDayFraction` and
`worldTimeTotalSeconds`, sampled alongside the player after the same server tick.
LocalPlayerPose exposes `world_day_fraction` and `world_total_seconds`; PoseHistory
interpolates phase across midnight and holds it on missing snapshots. The
isolated harness verified midpoint interpolation across midnight and continuing
world time while the chunk window stays unchanged.

## Session polling cost

The observed `open-world.csv` had 18 post-startup, roughly one-second samples:
session stage mean 0.3027 ms, stream/mesh mean 0.0089 ms, render mean 1.6964 ms.
These are sampled stage values, not per-frame tail-latency measurements.
LocalSession previously opened and parsed player-state JSON and checked the idle
interaction mailbox on every render frame. The first repair polled poses at 60 Hz with
fractional remainder, reuses its read buffer, skips parsing identical payloads,
and skips mailbox existence checks when no command is queued or in flight.
PoseHistory still advances at 1x elapsed time on every render frame; input
publication, server timing, and command ordering are unchanged.

A controlled 2,400-call check with a 1/240-second update argument and an isolated
server reduced pose reads from 2,400 to 600, with mean update cost 174.217 to
144.751 microseconds. This synthetic check is not a new live FPS claim. Scratch
instrumentation is `work/interaction-session/SessionPollingProbe.cpp`. The real
interaction/rejection/placement/restart harness passed again afterward using
`work/interaction-session/world-5` and `logs-5`. WorldStream generation/copy work
already runs outside its short publication lock; no speculative worker changes
were made from the available profile.


## Session I/O worker and measured hitch repair

A later live attribution captured a 63.885 ms frame with 61.025 ms in the
session stage and 2.602 ms in rendering (`logs/client/presentation-hitch-attribution.log`).
Reducing polling frequency alone did not remove synchronous disk stalls.
`App/LocalSession/SessionIo.cpp` now owns pose reads and JSON validation at up to
60 Hz, input and window writes, and ordered edit publication/acknowledgement.
The frame loop only swaps bounded mailboxes and advances PoseHistory every frame.
No mailbox lock spans filesystem I/O. Inputs overwrite obsolete unsent inputs,
are written once, and are discarded if already 250 ms old before writing; the
worker never generates input heartbeats while the main thread stalls. Existing
64-command unsent ordering, deletion acknowledgements, visible timeout handling,
and no automatic replay remain. Shutdown joins the worker before publishing the
server stop request. The public LocalSession API and JSON protocols are unchanged.

`tools/Source/ClientInteractionProbe/SessionIoProbe.cpp` injects a blocked worker
read while the main thread performs 2,400 mailbox submissions/polls. It verified
latest-only publication, dropping an input held for 300 ms, no repeated heartbeat,
no duplicate pose delivery, zero frame-thread read/write calls, worker join, and
exact 1x source-clock advancement. Final run reported 0.7 microseconds worst
mailbox call; timing is diagnostic, not a portable pass threshold. Scratch state
is `work/interaction-session/io-worker-probe-2`.

The isolated copied-server benchmark (`SessionPollingWorkerProbe.cpp` under
`work/interaction-session`) measured 2,400 update calls averaging 0.396333
microseconds, worst 35.6 microseconds, with zero frame-thread pose reads. The
synthetic loop ran without a frame sleep and is not a live FPS measurement.
The final real server interaction/rejection/placement/stop/restart harness passed
again using `world-worker-2` and `logs-worker-2` under `work/interaction-session`.
Direct native compilation passed conversion warnings as errors. The parent owns
the aggregate build and subsequent live frame-hitch comparison.
