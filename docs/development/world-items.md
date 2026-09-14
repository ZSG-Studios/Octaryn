# Authoritative world items

This implementation adds counted tosses and proximity pickup to the existing
creative catalog inventory. It does not turn the shortcut inventory into a
server-owned survival inventory, implement loot tables, or manufacture drops from
unconfirmed block breaks.

## Reference and ownership

The retained original source search found no dedicated dropped-item simulation
or pickup owner to reconnect. The local Pumpkin checkout contains selected world
and generation crates, rather than its item entity implementation; no Pumpkin
code was copied. Microsoft's [entity component guide](https://learn.microsoft.com/en-us/minecraft/creator/documents/entitycomponentsguide?view=minecraft-bedrock-stable)
describes gravity and collision as entity physics behavior. Minestom's
[first-party ItemEntity API](https://javadoc.minestom.net/net.minestom.server/net/minestom/server/entity/ItemEntity.html)
documents compatible-stack merging, pickup delay, and ticked item entities.
These establish the behavioral reference; the Octaryn implementation and numeric
limits below are explicit engine choices, not a claim of Minecraft version parity.

- `octaryn-server/Source/World/Items/WorldItems.cpp`: bounded native simulation,
  authoritative toss, collision, merge, expiry, and ordered pickup grants.
- `WorldItemsProcess.cs` and `NativeWorldItems.cs` in that owner: standalone
  process integration, module catalog/solidity callbacks, persistence, and files.
- `octaryn-shared/Source/World/Items/WorldItemWire.h`: versioned little-endian POD
  contract. The complete state is 15,440 bytes; an intent is 32 bytes.
- `octaryn-client/Source/WorldPresentation/WorldItems`: stoppable background file
  transport, durable toss outbox, immutable presentation snapshots, and receipts.
- `octaryn-client/Source/Rendering/WorldItems` and
  `octaryn-client/Shaders/WorldItems/WorldItems.slang`: standalone RHI rendering
  using existing catalog atlas layers, cube blocks and cutout sprite planes.

## Simulation and bounds

Only module-known, client-placeable, non-air block IDs can be tossed. Commands
carry a sequence, block ID, and count; position, orientation, and collision come
from the actual server player and block authority. Consecutive command IDs are
required, replays do not spawn again, command counts are 1–999, and tosses are
limited to one per 100 ms. Each world stack and pickup grant remains capped at
64. A whole creative inventory stack of 999 becomes 16 entities (15 of 64 and
one of 39) in one accepted command. The server checks room and all required
entity IDs before spawning any of them; failure preserves existing entities,
the next entity ID, and the toss cooldown. The rejection receipt still advances
the command watermark so replay cannot change its result. The world holds at
most 256 stacks and 64 pending pickup grants.

The native item has a quarter-block collision box. Gravity and axis-separated
solid-voxel collision run in substeps of at most 1/120 second, with a maximum
authority delta of 250 ms. Toss velocity follows authoritative yaw/pitch.
If a block is placed around an item, a bounded 36-candidate search resolves it
to nearby clear space within 1.5 blocks. Fully enclosed stacks remain intact;
the search cannot teleport them arbitrarily far or collect them through solid.
Compatible nearby stacks merge without exceeding 64, retaining the longer
pickup protection and younger age. Toss protection lasts two seconds; active
items expire after 300 seconds. Physics and age freeze beyond 64 horizontal
blocks from the local player. This slice does not implement fluid buoyancy,
hazard damage, multiplayer ownership, or item-specific collision shapes.

## Durable ownership transfer

`world/world_items.bin` stores the complete item state and a SHA-256 checksum.
Atomic replacement with a durable file flush precedes publishing a command
receipt or pickup grant. Movement is checkpointed once per second and on clean
shutdown; receipt transactions are immediate. A failed save leaves an explicit
retry pending and cannot publish the uncommitted credit. The publication latch
also survives a failure when pickup removes the final world item.

The server writes `runtime/world_items.snapshot` at up to 30 Hz while items
exist, and on transactions. Empty idle worlds and unchanged pending grants do
not trigger recurring snapshot or checkpoint writes. The independent client
worker uses the existing precise stoppable wait; no per-frame file I/O was
added to movement or rendering. Windows reads share deletion so atomic server
publication is not blocked by the reader.

The client first saves `client/world_items.pending`, then atomically publishes
`runtime/world_items.intent`. UI reserves and saves the selected cursor units
before submission. A drop receipt stays available until UI saves its debit or
rejection restoration plus its drop receipt watermark. Only then may the
transport acknowledge that receipt. Restart reuses the durable command ID.

Pickup removes a nearby stack into a durable grant for the local player. Grants
have monotonic, persisted IDs and are exposed oldest first. UI credits a whole
grant only if capacity permits, saving its inventory and grant watermark
together before acknowledging. Duplicate committed IDs never add units again.
An unaccepted grant remains persisted and retryable. At grant capacity, further
items remain in the world. Full inventory therefore holds pending server grants;
it does not yet negotiate capacity to leave every reserved pickup visibly on
the ground as Minecraft does.

## Presentation and validation

Presentation interpolates between received item positions for at most 100 ms;
it never predicts beyond the newest authoritative position. Bob and spin are
visual only. The shader uses per-face atlas mappings and emits HDR color plus
the same unjittered object-motion convention as the player. Previous item and
camera state are committed only after successful graphics submission. New IDs
reset motion history. Inline records are bounded to 256 and avoid per-frame
GPU buffer creation. Item magnification remains crisp; minification uses the
atlas's edge-clamped filtered mip sampler and the renderer's temporal mip bias.

The new native probe covers counted directional toss, replay, catalog rejection,
rate/capacity limits, floor collision, merge conservation, pickup delay, expiry,
region freezing, ordered acknowledgement, and grant backpressure. The actual
client worker probe covers durable outbox order, restart receipts, oldest-first
pickup acknowledgement and malformed snapshot retention. The existing managed
server world-block probe now includes actual module/native/process save retry,
restart, and pending-grant qualification, including Windows sharing contention.

`--validate-world-items` adds an explicit domain-level packaged scenario. It
requires an isolated `OCTARYN_CLIENT_WORLD_PATH` and a capture destination in
`OCTARYN_CLIENT_CAPTURE_PATH`. The scenario reserves one real inventory unit,
submits through the normal worker, waits for an actual server item and saved UI
debit, arms capture after the item leaves the eye, then sends real player inputs
to approach it. Completion requires inventory restoration and the server's
published grant acknowledgement. It does not inject OS input, fake server
snapshots, or fabricate renderer geometry. GPU captures remain available for
visual inspection alongside the transport and persistence evidence.

The canonical packaged runner retains a unique world, settings, inventory, logs,
BMP and `result.json`; it checks the binary save checksum and both persisted
inventory watermarks independently of the client's pass marker:

```powershell
python.exe tools/validation/validate_world_items.py `
  --client-bundle-root build/release-windows/client/bundle `
  --evidence-root logs/client/world-items --backend dx12 --upscaler off
```

Use `--backend vulkan` only with a package that supports that selected backend.
Optional upscaler values are `off`, `native`, `quality`, `balanced`, `performance`
and `ultra-performance`; the runner verifies the logged active mode. It bounds
the process to 110 seconds and controls only the client/server it launches.

Add `--whole-stack` to seed one isolated client inventory slot with 999 grass
items. The production UI reserves that stack and sends one actual toss command.
Before capture, qualification requires the authoritative snapshot to contain
16 entities: 15 stacks of 64 and one of 39. It then approaches the remaining
items through normal player input, checks 16 ordered saved inventory credits,
and requires all 999 units restored with server acknowledgement 16. The runner
also verifies the final server save contains no remaining items or grants,
`next_item=17`, `next_grant=17`, and one accepted command for 999. The fixture
seeds inventory only; item snapshots and rendered geometry come from production.

## Current verification — 2026-09-13

The coordinated final CPU qualification passed 101 native world-item checks,
20 client transport checks, and 71 inventory checks. The managed process probe
also passed its actual native/save/retry/restart contract checks.

The following packaged runs passed with RHI validation enabled, a real hidden
SDL surface, and a captured 1280×720 GPU frame. Each evidence directory retains
`result.json`, `client.log`, `frame.bmp`, isolated world saves, and inventory.

| Backend / upscaler | Actual toss and pickup | Evidence directory |
| --- | --- | --- |
| DX12 / Quality | 999 units, 16 entities and credits, acknowledgement 16 | `logs/client/validation/presentation-final/world-items-7xz_qnwk` |
| Vulkan / Quality | 999 units, 16 entities and credits, acknowledgement 16 | `logs/client/validation/presentation-final/world-items-pgc0z9yk` |
| DX12 / Off | One unit, one entity and credit, acknowledgement 1 | `logs/client/validation/presentation-final/world-items-u0oiu7zp` |

All three records identify client SHA-256
`fab289ea791b1ccc5d787742e842ade7ab9818c58772afe8bfca20970c7e628c`.
The whole-stack cases independently confirm saved inventory conservation from
999 to zero after debit and back to 999 after pickup. The single-item case
confirms 999 to 998 and back to 999. These are bounded Windows packaged
integration results; they do not qualify multiplayer, every terrain collision
case, other desktop platforms, long-duration behavior, or rendering performance.

The subsequent strict Vulkan run is
`logs/client/validation/presentation-native-final/world-items-sd7eae2o`.
It explicitly loads Khronos Core + Synchronization validation and passes the
same 999-unit/16-entity conservation check with no reported warnings or errors,
after repairing render-target clear usage and unused raster varyings. Its client
SHA-256 is `d10221d336cc97eff4eb96b45d3a635aed7f92bf7f0ea581809190dfd38192a9`.
The older Vulkan row above is RHI-level functional evidence; it did not explicitly
load the native layer and is not used as the final native validation proof.
