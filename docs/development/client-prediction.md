# Owning-player prediction and reconciliation

Requested 2026-09-17. This supersedes the owning-player presentation policy of
waiting for authoritative snapshots before showing movement. The server remains
authoritative; prediction is provisional client presentation and collision state.

## Required behavior

- After the initial authoritative spawn/baseline, local walking, looking and jump
  input affect the owning player immediately, without a network round trip.
- Each simulation input has a monotonic sequence. The server acknowledges the
  last input actually simulated, not merely received or copied into a mailbox.
- The client retains bounded unacknowledged input history. On authoritative state,
  restore the complete movement state and replay only the remaining inputs.
- Jump press/release state survives reconciliation without lost short taps or
  duplicate impulses. Retaining an event for delivery must not delay its local
  predicted effect.
- Physics corrections affect the predicted body immediately; small presentation
  errors can blend visually. Teleports/session changes reset prediction history.
- Interpolation of other players remains separate from owning-player prediction.
- Simulation cadence and validation remain server-controlled. Client-provided
  sequence numbers or timing must not permit faster movement or extra jumps.
- Shared module contracts do not expose Jolt, LES internals or native pointers.

## World interaction

Immediate local feedback is distinct from a committed result. Predicted visual
changes must resolve against explicit authoritative acceptance/rejection. Durable
item reservations, count conservation and save-before-ack ordering remain required.
An input-file deletion is not proof that a block edit was accepted or persisted.

## Qualification

Measure input onset to predicted movement/takeoff separately from input-to-server
acknowledgement. Exercise fixed-step movement, short taps, held jump, collision,
reconciliation/replay, rejected actions and reconnect history resets. Repeat with
bounded, seeded packet delay, jitter and loss. Record correction sizes and playback
holds; passing an eventual jump alone does not establish responsive control.

Implementation and runtime evidence are recorded below.

### Completion gates

- Local and remote jump probes: ten cases each, input-to-local-takeoff at most
  50 ms, no lost short taps, repeat impulses, history overflow or wall penetration.
- Repeat the remote cases with seeded 30–60 ms one-way delay and 2% packet loss;
  report acknowledgement latency, replay work and maximum correction separately.
- Fixed-step prediction must present smoothly above 60 Hz, without changing the
  authoritative collision state or delaying local look until acknowledgement.
- Exercise actual accepted and rejected block commands through the production
  receipt path, including unchanged-revision rejection and reconnect cleanup.
- Inspect GPU captures of provisional toss presentation and authoritative handoff;
  retain item count conservation and durable transaction ordering.
- Repeat local and remote menu/rejoin qualification with isolated save directories.
- Arch player authority is qualified with two independent ECS actors. Multiple
  network peers and remote-avatar presentation remain separate gates. Native
  replay must not be reported as LES-owned input history/pawn prediction.
- Linux hardware and macOS runtime qualification require those environments.
  Windows results cannot close those gates.

Before packaging this change, existing save directories and partial recovery
evidence were copied with per-file SHA-256 verification to
`saves/backups/prediction-20260917-032526`. This protects surviving data; it does
not recover the authoritative world deleted during the earlier packaging incident.

### Baseline before prediction

`logs/client/remote-jump-20260917-025318.log`: ten loopback jumps passed physics
qualification, but presented takeoff latency was 215.0–252.3 ms (mean 233.7 ms).
There were no playback underruns in that run. This distinguishes the deliberate
authoritative-presentation delay from packet-loss stalls or failed jump input.

### Integrated qualification in progress

- `logs/client/local-prediction-20260917-034732.log`: ten local jumps passed the
  50 ms response budget through the packaged supervised server.
- `logs/client/remote-jump-20260917-033940.log`: ten remote loopback jumps passed;
  zero history overflow, playback holds and measured reconciliation displacement.
- `logs/client/remote-jump-20260917-034035.log`: the first impaired run failed
  standing landing-height qualification after an acknowledgement stall. Nine
  later cases passed. This is a recorded failure, not impairment qualification.
- `logs/client/prediction-items/world-items-jfly9n71`: actual DX12 provisional
  and accepted captures inspected, one-item pickup returned the initial count.
- `logs/client/prediction-items/world-items-qltgch50`: 999-item provisional toss,
  authoritative split and durable pickup conservation passed.
- `logs/client/prediction-rejoin/rejoin-2ipwvblv` and `rejoin-7axv05nl`: local
  and dedicated sessions each passed three connections and two menu returns.
  The remote GPU capture was inspected for terrain, foliage and HUD continuity.

Local player animation now uses a client presentation clock so walk and attack
animation do not pause between server snapshots. World time remains authoritative.

### Impaired transport repair

Movement no longer shares the reliable-ordered LES action/request stream. Bounded
unreliable command datagrams on the authenticated LiteNetLib peer carry redundant
unacknowledged history. Loss/reordering is repaired by periodic retransmission;
the server deduplicates commands and retains its fixed authoritative time budget.
Protocol version 4 requires matching client/server bundles. LES continues owning
entity replication and reliable action/result delivery, not the native prediction
clock or history.

- `logs/client/remote-jump-20260917-035931.log`: 30–60 ms one-way jitter, 2% loss,
  ten jumps passed, mean takeoff 15.1 ms, maximum 17.7 ms. Maximum pending 21;
  zero correction displacement, holds and overflow.
- `logs/client/remote-jump-20260917-040045.log`: 100–150 ms one-way jitter, 5% loss,
  ten jumps passed, mean takeoff 14.1 ms, maximum 16.7 ms. Maximum pending 41;
  zero correction displacement, holds and overflow.
- Both bounded UDP proxies reported no queue-bound drops or send errors. These
  are seeded Windows qualifications, not every Internet condition.
- Protocol-4 remote rejoin: `logs/client/prediction-rejoin/rejoin-x5as7988` passed
  all three sessions and two menu returns.
- Integrated bundles plus static checks: `logs/build/prediction-datagram-bundles.log`.

The earlier failed impairment run is retained above. A forced grounded rollback
also has a targeted presentation regression: physics grounds immediately while
the visual descent finishes without claiming a floating pose is grounded.

### Input events and authority ownership

The real SDL event owner now retains ordered jump transitions. A press and release
drained in the same 30 Hz frame produces one jump; final keyboard-state sampling
alone previously discarded it. Constructed-event qualification invokes that same
parser without injecting OS input and covers UI consumption, repeat keydown,
focus cancellation and flight controls. Evidence:
`logs/client/prediction-input-events-20260917.log`.

Server player simulation now runs through private Arch components and an actual
query system, including accepted input, complete movement state and native body
lifecycle. The two-actor, save/generation and fixed-budget qualifications are in
[player ECS authority](player-ecs-authority.md). This does not enable multiple
network peers or remote avatars by itself.

The static validation aggregate also checks identical ordered LES fields, RPC
types and RPC registrations in the two owner wire copies.

## Final integrated Windows qualification

Matching current bundles use **RemoteProtocol 5** and **OCSTRM01 snapshot v3**.
Snapshot v3 carries an authoritative edit watermark independently of the content
hash used for caching. Accepted overlays now survive stale baseline delivery;
metadata-only coverage retires them without remeshing unchanged terrain.

Build and static validation: `logs/build/prediction-final-bundles.log`.
Existing world-stream regression: `logs/build/prediction-world-stream.log`.
All 139 touched/new source files checked at this stage were below 500 lines.

| Qualification | Evidence | Result |
|---|---|---|
| Local jump response after ECS integration | `logs/client/local-prediction-final-20260917-043131.log` | 10/10, 50 ms budget |
| Remote jumps, 30–60 ms one-way / 2% loss | `logs/tools/remote-jump-20260917-042841-11dbe012.json` | 10/10; mean 13.45 ms, max 15.3 ms; zero holds, corrections or overflow |
| Remote jumps, 100–150 ms one-way / 5% loss | `logs/tools/remote-jump-20260917-043050-e2252611.json` | 10/10; mean 15.47 ms, max 18.0 ms; zero holds, corrections or overflow |
| Remote block actions, 30–60 ms / 2% loss | `logs/tools/remote-blocks-20260917-042841-f4bc309a.json` | 7 receipts; exact rejection rollback, revision coverage, movement gating, ack retirement; zero feedback failures |
| DX12 block prediction and rollback | `logs/client/prediction-blocks/block-actions-8leq67gf` | Seven actual GPU captures inspected: original, pending reject, rollback, pending accept, accepted break, pending restore, restored |
| DX12 provisional 999-item toss and pickup | `logs/client/prediction-items-final/world-items-g_l2rg1k` | Count conservation and durable grant acknowledgements passed |
| Local menu/rejoin | `logs/client/prediction-rejoin-final/rejoin-2s1480gu` | Three sessions, two menu returns |
| Dedicated menu/rejoin | `logs/client/prediction-rejoin-final/rejoin-u_ubtn1l` | Three sessions, two menu returns; server exit 0 |

Reproducible impairment tooling is tracked under `tools/validation/network`;
see [network qualification tools](network-qualification-tools.md).
GPU entry points are `validate_block_actions.py`, `validate_world_items.py
--provisional`, and `validate_session_rejoin.py` under `tools/validation`.

These results establish the listed Windows/single-peer cases, not an unrestricted
multiplayer or cross-platform completion claim. The deleted authoritative world
remains unrecovered; surviving saves and partial evidence remain preserved.

Normal startup after qualification launched dedicated PID 14224 and client PID
9424 without diagnostic flags or output redirection. The server loaded the existing
`saves/dedicated` player save, accepted the client on port 17531 and advanced ticks.
The client uses `saves/dedicated-client`. Launch details are recorded in
`logs/tools/prediction-normal-launch.json`; these PIDs describe that launch only.

### Shared native movement kernel (2026-09-17)

`octaryn_character_motion` is a static host-only native library under
`octaryn-shared/Source/Libraries/CharacterMotion`, analogous to `NativeJobs`.
It is shared implementation for the native client and server hosts, not a managed
shared/module API. Jolt is a private target dependency; no Jolt types occur in
`CharacterMotion.h` or `CharacterGeometry.h`. The client links this kernel rather
than the authoritative player session/persistence DLL.

Canonical contract: namespace `octaryn::character_motion`, `Input`, `State`,
`SolidQuery = uint32_t (*)(void*, int32_t, int32_t, int32_t)`, and
`void step(const Input&, float deltaSeconds, State&, SolidQuery, void*)`.
Input/state fields preserve the existing server native structures, including
`selected_block` and `jump_held`. Query results retain the server packed block
format: low 16 bits block ID, bit 16 solidity. Callers must supply the same solid
world view. State contains the complete cross-step movement state; the Jolt
character/world is reconstructed per step with no hidden replay history.

Source-to-destination map (all source paths start at
`octaryn-server/Source/Simulation/Players`):

| Source | Destination / ownership |
| --- | --- |
| `PlayerMovement.{h,cpp}` | Shared `CharacterMotion/PlayerMovement.{h,cpp}`: fly/walk integration |
| `PlayerJoltMovement.{h,cpp}` | Shared `CharacterMotion/PlayerJoltMovement.{h,cpp}`: private Jolt movement |
| `PlayerJoltWorld.{h,cpp}` | Shared `CharacterMotion/PlayerJoltWorld.{h,cpp}`: private collision scan, support and penetration resolution |
| Movement portion of `PlayerSimulation.cpp` | Shared `CharacterMotion/CharacterMotion.cpp`: camera normalization, delta clamping and substeps |
| Geometry constants from `PlayerJoltWorld.h` | Shared `CharacterMotion/CharacterGeometry.h`, reused by server placement policy |
| `PlayerSimulation.cpp`, `PlayerPlacementPolicy.cpp` | Server authority adapters retained; existing exported ABI names/layout unchanged |

Extraction uses the current active implementation and pinned Jolt dependency;
the original historical reference was unavailable for this focused task. The
existing sharp-box seam fix, jump-edge handling, fly/control mode behavior and
authoritative block-query semantics are retained. Server double-precision delta
clamping remains before conversion into the native float kernel contract.

Verification: targeted Windows native build of
`octaryn_server_player_simulation_probe`, unchanged full player probe (including
64 voxel seam cases), and unchanged `--obstacles` qualification all pass. The
obstacle run passes all 18 cases across 30/60/120 Hz. Evidence:
`logs/server/character-motion-native-probe.log` and
`logs/server/character-motion-obstacles.log`. No packaged bundle rebuild or
client graphics/network qualification was part of this kernel extraction.
