# Server player ECS authority

Qualified on Windows x64, 2026-09-17.

`ModuleActivator` owns one `PlayerSimulationWorld`, shared by player adapters.
The server's existing Arch host package owns entities with identity/generation,
accepted frame/input, complete authoritative `PlayerState`, and a private native
session component. `PlayerSimulationSystem` runs an Arch inline query that invokes
the retained native CharacterMotion/Jolt step and writes the state component.
Replication snapshots and persisted pose/selection are read from that component.
No Arch or native handle types enter module/public contracts.

`PlayerController` is the existing call-site adapter for command consumption,
spawn alignment, collision queries, logging, and save lifecycle. The current world
still loads/saves player ID 1. Other IDs share the same ECS world and use distinct
native sessions and identity-keyed save entries. Removing an entity invalidates
its generation without resetting its siblings.

## Tick contract

- The existing server clock and `PlayerCommandQueue` own sequence acceptance and
  fixed time credit. ECS introduces no timer, thread, or competing world tick.
- `StepOne` queues and executes just the named actor's accepted frame.
- `Queue` permits one pending frame per actor; attempting to overwrite it fails.
- `StepQueued` steps each queued actor once and leaves unqueued actors unchanged.
  A second drain does not repeat simulation. Step-one does not drain other actors.
- These APIs accept already-selected commands; they do not independently dedupe
  frame indices or grant time credit.

## Qualification

Isolated managed build outputs are preserved under
`build/release-windows/tools/validation/ecs-authority`. Server,
`Octaryn.PlayerCommandsProbe`, and `Octaryn.ServerWorldBlocksProbe` managed builds
passed with zero warnings/errors. Native qualification loaded the existing
`build/release-windows/server/native/bin` player, block-store, and persistence DLLs.
Canonical bundles were not rebuilt.

Run the player command probe with `--player-ecs <save-directory>`, setting
`OCTARYN_SERVER_PLAYER_SIMULATION_LIBRARY`, `OCTARYN_SERVER_BLOCK_STORE_LIBRARY`,
and `OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY` to those DLLs.

`logs/server/player-ecs-qualification.log` records passing assertions for:

- Two ECS actors moving independently under distinct commands.
- Batch-once execution, isolated step-one, rejected pending-command overwrite,
  and rejected stale generations.
- Separate ID-keyed saves sourced from authoritative components.
- 300 exact complete-state comparisons against native CharacterMotion/Jolt;
  8.083 units of grounded voxel-seam traversal before jump, then press/release
  and landing, while the second actor's complete state remains unchanged.
- Actual command-queue-to-ECS execution: no initial client-granted credit,
  exactly six actor steps for 100 ms of wall credit, and 100 duplicate packets
  granting no additional motion.
- Controller snapshot publication, disposal/save/recreation, and continued
  sibling-body operation in the shared world.

The existing full player-command qualification also passed, recorded in
`logs/server/player-ecs-command-budget.log`, including duplicate/gap handling,
bounded catchup, stale retirement, double-client-rate limiting and impaired
datagram delivery.

This qualifies server gameplay ECS ownership with independent actors. Multi-peer
session admission and remote-avatar replication remain separate work. This pass
does not replace the separately recorded live prediction latency qualification
or establish Linux/macOS runtime support.
