# Fixed-command player prediction

`player_input.json` is version 2: `{ "version": 2, "commands": [...] }`.
Each command contains `frameIndex` (uint64, starts at 1), `flags`, `controller`,
`moveX`, `moveY`, `moveZ`, `cameraPitch`, `cameraYaw`, `relativeMouse`.
No client dt or position is used. Floats must be finite; movement axes [-1,1].
Client resends oldest unacknowledged contiguous commands: max batch 64,
max outstanding 256. Duplicate commands are ignored; gaps are not acknowledged.
One command is exactly 1/60 second, paced by server monotonic wall time.
At most eight steps execute per poll; at most eight steps of credit are retained.
No commands: freeze the actor for up to 250ms without a new command; never
re-simulate held input during transport delay. Retained wall credit permits bounded
catch-up when commands arrive. After 250ms, run autonomous neutral actor steps
(preserve fly mode and view, clear movement/actions). These steps have no command ID.
Queued commands older than 250ms are retired without replay; their contiguous
IDs are acknowledged with the next successfully completed actor step. This
coalesces expired backlog rather than leaving commands permanently late.
Ack means fully resolved (simulated or expired), not necessarily simulated.
`sourceTick`/`simulationTick` counts all fixed steps, including idle;
`sourceSeconds`/`simulationTime` is actor tick/60. World time has an independent
server wall-clock fixed-step budget: never advanced again for actor catch-up.
All command state and budget reset per session. LES input/prediction clock is unused.
State includes position, pitch/yaw, velocities, control mode, grounded, `jumpHeld`.
RemoteProtocol version/key is 4. Native prediction replays only unacknowledged commands.

Block authority can read `ChunkStreamProcessBridge.ConsumedPlayerCommand` and call
`EvaluateCommandDependency(inputFrame, waitingSeconds)`: Ready for resolved input,
Wait for future input within 256 and 500ms, Reject otherwise. The block owner
retains at most one pending intent per mailbox and starts its timeout on receipt.
Reach validation runs against the authoritative body only once Ready.

The implemented `BlockCommandAdmission` bridge reads top-level `movementFrameID`
from the block intent (absent means 0 for explicit tooling), calls
`ModuleActivator.AdmitBlockInteraction`, and reserves before submission/deletion.
Queue-full retries retain the same reservations; large batches progress in bounded
receipt-capacity slices. Future/out-of-window dependencies produce rejected
receipts through `RejectAdmittedBlockInteraction`. All submitted command camera
positions are replaced by the authoritative body position, for local and remote.
Remote attach/detach calls Begin/EndBlockReceiptSession; local stream setup begins
the receipt session before reading its first stream or block intent. Module save hooks own the
durable publication barrier.

LES RPC registration order is snapshot, block-consumption ack, welcome, item
snapshot, block-results. The final span forwards `block_results.json` unchanged;
intent kind 6 forwards `block_results_ack.json` unchanged. Neither transport
fabricates receipts or treats a file-consumption ack as authoritative acceptance.

## Movement transport (protocol 4)

The pinned LES 1.2.2 `HumanControllerLogic.SendRequestStruct` sends reliable-ordered
requests, emits a server response RPC for every received request, and ignores sends
in rollback state. Previously movement shared that stream, the client cached bytes
regardless of hand-off, and the server collapsed arrivals into one latest mailbox.
That combined head-of-line blocking, stale intermediate batches, and suppressed
unchanged retransmission. Protocol 4 removes those movement-only dependencies.

Movement bypasses the reliable entity-request stream and latest-intent mailbox.
The existing authenticated, welcomed LES-associated LiteNetLib peer sends unreliable
command datagrams. Every ~1/60 second, the client re-reads and resends the complete
oldest-unacknowledged JSON batch, including unchanged batches. The peer MTU and a
20-command cap bound each datagram; batches split into multiple datagrams.
Server network dispatch admits each datagram directly into the same bounded
fixed-command queue used by local JSON input. There is no new simulation clock.
Reliable kind-2 movement requests are no longer admitted. State and other intents
continue using LES. Protocol/key 4 prevents silently pairing with kind-2 clients.

Binary envelope, little-endian: byte `0x50`, byte protocol version (4), uint16
command count (1..20), then 40 bytes per command in this order: uint64 frameIndex;
uint32 flags/controller; float32 moveX/moveY/moveZ/cameraPitch/cameraYaw; int32
relativeMouse. Exact length, finite axes/view, flags, sequence and window checks
precede admission. Maximum payload is 804 bytes, further bounded by peer MTU.
Ack remains the coherent authoritative pose's consumed-command watermark.

Timing uses each version-2 command's first-send identity. Retransmissions never
reset edge timestamps; separate `input_ack` records measure each press/release.
Server `server_command_timing` distinguishes received, contiguous accepted,
consumed ack, queue depth and oldest age. Expiration is explicitly logged.

Focused datagram/queue/native-body qualification with seed 20260917, independent
30-60ms delay each direction and 2% loss: all 600 commands consumed/acknowledged;
maximum client backlog 11; maximum acknowledgement-progress gap 70ms; 9 dropped
datagrams; maximum actual payload 444 bytes; one-frame jump edge preserved once.
Also checked unchanged-batch retries after lost head/out-of-order tail, duplicates,
truncated envelopes and wrong versions. Log:
`logs/server/player-commands-datagrams-probe.log`. This controlled impairment test
does not replace the main session's full LES/proxy/graphical rerun.

## Focused verification (2026-09-17)

`Octaryn.PlayerCommandsProbe` uses a controlled monotonic clock and the actual
native `octaryn_server_player_step` export. Results are in
`logs/server/player-commands-probe.log`: delayed six-command catch-up; duplicate
flood of 100; 64-command burst bounded to six steps in 100ms; expired backlog
retirement and fresh recovery; gap rejection/recovery; same-batch jump press and
release; 256 outstanding cap; reset; and independent world/actor budgets.
At 120 client commands/sec over two seconds, the native body executed exactly
120 fixed steps and moved 19.999996 units, rather than 240 steps. Client dt of
1,000,000 seconds was ignored throughout. Both managed hosts build successfully;
the focused native player-simulation target builds. The 15 SyncVar declarations
and five RPC registrations match across client/server. Network/GPU qualification
is performed separately by the main integration session.
