# Remote-session repair — 2026-09-17

The Windows remote path uses LiteNetLib transport and LiteEntitySystem session
entities/controllers. Arch holds connection, pending-intent and publication
state. Native Jolt remains authoritative; native pose history presents received
source-time states. LES input prediction and broad gameplay ECS integration are
not qualified by this repair.

## Repairs

- Forward binary world-item intents and snapshots through the remote session.
- Step the server-owned item process remotely, keeping its durable save in the
  world directory and transient files in the session directory.
- Retain pending intents when atomic file replacement encounters contention.
- Ignore disconnect callbacks belonging to rejected peers.
- Honor the explicit client world path in remote CLI qualification.
- Emit immediate, unbuffered client startup output and main-menu readiness.
- Advance the connection key/protocol to v2 for the changed LES wire layout.
- Ordinary server startup now hosts a dedicated session on port 17531 and honors
  `--world-dir`. Supervised local streaming keeps its existing path; readiness
  tooling requests `--one-shot` explicitly. Requested shutdown exits successfully.

The console output from redirected test launches is captured in log files.
Those launches do not demonstrate that an ordinary console window displays
text. File logging and visible-console verification are separate checks.

## Qualification

- Windows release client/server bundle build passed:
  `logs/build/remote-items-bundles.log`.
- Remote single-item toss/pickup passed with inventory `999 → 998 → 999`,
  grant 1 and server acknowledgement 1. Client exited 0 after 330 frames and
  81 columns. Evidence: `logs/client/remote-items-20260917-015106.log`.
  The GPU capture was inspected: terrain, vegetation, UI and the dropped block
  are present. This is Windows DX12/RX 9070 XT evidence.
- Real remote handshake, pose/snapshot delivery, block-consumption acknowledgement
  and reconnect passed: `logs/client/remote-loopback-20260917.log`.
  Block-consumption acknowledgement is not a durable accepted-placement receipt.
- Remote full-stack toss/pickup also passed: 999 items split into 16 entities
  (15 × 64 plus 39), all 999 restored, grant/server acknowledgement 16.
  Evidence: `logs/client/remote-items-20260917-015606.log`.
- Actual nonredirected Windows console buffers contained visible text:
  `logs/server/console-attached-before.json` and
  `logs/client/console-attached-after.json`. The rebuilt client includes the new
  startup and menu-ready markers. No input injection or window activation was used.
- Client mailbox contention checks passed snapshot/item retry, acknowledgement
  retry, newer-intent preservation and authoritative-tick pose retry.
- Rebuilt ordinary dedicated startup displayed readiness in its actual console,
  bound port 17531 and exited 0 on the shutdown request:
  `logs/server/console-attached-default-after.json`.

## Jump input delivery

The expanded probe reproduced intermittent loss of 20 ms taps in the latest-value
input path. A bounded press/release queue now retains each edge until a successful
server input tick acknowledges its frame. The consumed-input acknowledgement is
distinct from heartbeat frame numbers and is replicated through LES.

- Local: 10/10 cases passed, including 20 ms taps standing/forward, holding through
  landing without auto-repeat, and pressing again after release.
  `logs/client/jump-ack-held-20260917-015637/probe.log`.
- Remote: 10/10 cases passed, server exited 0.
  `logs/client/remote-jump-20260917-020630.log`.
- Post-release checks distinguish upward takeoff from walking off a lower ledge.
  The initial remote check treated any airborne state as a repeat and was corrected.
- Current loopback input-to-ack timing is approximately 107–155 ms; presented
  takeoff is approximately 250–270 ms. This is not a low-latency prediction claim.

The supported LES fixed buffer allowances were then set to zero, retaining its
adaptive jitter allowance and the native 50 ms presentation history. In the
follow-up `logs/client/remote-jump-20260917-021005.log`, all 10 cases passed;
mean input-to-ack fell from 119.4 to 88.5 ms and mean presented takeoff from
257.6 to 235.2 ms. These are measured loopback results, not Internet guarantees.

Bidirectional UDP impairment also passed 10/10 cases with seeded 30–60 ms
one-way delay and 2% loss. This used a bounded external proxy because the pinned
LiteNetLib Release package's built-in simulation is inactive. Evidence:
`logs/client/remote-jump-20260917-021543.log` and
`logs/tools/remote-jump-proxy-20260917-021543.json`; no queue-bound drops or send
errors occurred, and both processes exited 0.

Native playback instrumentation measured one underrun in each final run:
64 ms held in the unimpaired run and 9.8 ms held in the impaired run. These are
separate workload samples, not evidence that impairment improves playback or
that all stutters are eliminated. See `remote-jump-20260917-021524.log` and
`remote-jump-20260917-021543.log` under `logs/client/`.

Broader Internet conditions, LES prediction and multi-player entity authority
remain unqualified. Block-face/top-edge collision qualification is separate from
input delivery.

## Block-face jump snag

The real BlockStore/Jolt obstacle probe reproduced a voxel-seam contact reporting
ground support during ascent. That canceled upward momentum at a rise of only
0.827 blocks, leaving the player against a one-block wall. Grounded movement now
requires non-rising velocity; landing additionally requires voxel floor support.
Solid collision, penetration correction and the disabled stair-step bypass remain
intact.

- 18/18 obstacle cases passed: front, seam, oblique, edge, sprint and a blocking
  two-block wall, each at 30/60/120 Hz. The reproduced seam case now rises 1.267
  blocks and clears in 0.65 seconds without measured overlap or ascent stall.
- Full native player simulation regression passed, including 64 seam cases.
- Local transport jump regression passed 10/10 after the physics change.
- Final packaged remote jump regression passed 10/10, and the full 999-item
  remote toss/pickup passed again with both processes exiting 0:
  `logs/client/remote-jump-20260917-021005.log` and
  `logs/client/remote-items-20260917-021103.log`.
- Evidence: `logs/client/jump-obstacle-before.log`,
  `logs/client/jump-obstacle-final.log`,
  `logs/client/jump-physics-regression.log`, and
  `logs/client/jump-obstacle-local-20260917-020755/probe.log`.

Obstacle qualification directly exercises server authority. It is not a claim
that every generated terrain arrangement or remote graphical obstacle course has
been tested.

## Final checks

- Windows release packages built; native physics DLL hashes match the dedicated
  bundle and the client bundle's supervised-server payload.
- Canonical packaged local jump probe: 10/10,
  `logs/client/local-jump-final-20260917.log`.
- CPU validation passed: `logs/build/remote-repair-cpu.log`.
- Static validation passed: `logs/build/remote-repair-static-final.log`.
- Touched source line limits and `git diff --check` passed.
- Linux/macOS runtime qualification was not performed.

## Normal dedicated-server logging

Dedicated startup defaults to concise logging: lifecycle/errors/edits, changed
chunk windows and a `server_status` summary every five seconds. Per-tick pose,
input, timing and per-packet snapshot diagnostics are omitted from normal output.
Set `OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY=0` before launch to opt into full
diagnostic output. One-shot qualification retains its existing trace behavior.

Qualification: `logs/server/remote-jump-20260917-022724.log` contains 36 lines
and four status summaries across a 10/10 passing jump run. The terrain publication
guard now applies to recurring remote sessions regardless of the native startup
mode. Full-trace qualification `remote-jump-20260917-022757.log` recorded only two
terrain publications (initial baseline and center change), while 927 authority
updates continued and world time advanced beyond the last terrain snapshot.
The native-backed publication/fluids/persistence probe passed after updating its
callers: `logs/build/idle-publication-probe.log`.

## Packaging incident during follow-up

The old server packaging command recursively removed the bundle directory. During
the 02:27 rebuild it also removed the ordinary launch's default
`build/release-windows/server/bundle/octaryn-world` directory. No complete
authoritative save backup was found. Remaining client-side inventory, chunk-view
request and item snapshot were preserved at
`saves/recovery/20260917-dedicated-world-client-cache`; these files are evidence,
not a recovered world. Unrelated saves were left untouched.

Server packaging now publishes to a sibling staging directory and uses the atomic
bundle installer. Replacement retains the entire previous bundle under
`server/bundle.retired/<id>/` and carries `octaryn-world` forward with byte-hash
verification. Failed copying, verification or installation restores the old
bundle. Disposable installer qualification passed 14 cases with one skipped;
the generated CMake server command was regenerated and inspected to confirm it
no longer removes the live bundle. This protection does not recover the save
removed by the earlier command.
