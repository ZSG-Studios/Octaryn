# Networking recovery references

The recent implementation is preserved under:

C:\Users\Rose-X\Documents\Octaryn-Backups\2026-09-13-before-old-engine\workspace

All paths below are relative to that workspace. Copy selected behavior into the
active engine with a focused change; leave the backup intact.

| Preserved source | Recovery value |
| --- | --- |
| octaryn-client/Source/Managed/Presentation/RenderSnapshotBuffer.cs | Monotonic source-time history; stale/duplicate rejection; 1x playback; startup buffer; outage hold/refill; velocity-aware Hermite position and quaternion slerp; contact/reversal linear fallback; teleport handling; reconnect reset; embedded self-tests. |
| octaryn-client/Source/Managed/Networking/PeerConnection.cs | Raw authoritative states feed presentation directly rather than interpolating an already interpolated LES value. Buffer configuration and connection reset. |
| libraries/networking/Source/Managed/Replication/DronePawn.cs | Replicates absolute physics time with pose and velocity; publishes a coherent body's state. Protocol changes require coordinated client/server versioning. |
| libraries/networking/Source/Managed/NetworkPeer.cs | Transport wakeups do not flush artificial latency queues; reliable baseline bypass; unreliable excess dropped rather than accumulating stale backlog; disposal. |
| libraries/networking/Source/Managed/NetworkLinkSimulator.cs | Bounded impairment queues, flush budgets, stale-packet dropping, pooled-buffer cleanup, and counters. |
| octaryn-server/Source/Managed/Networking/Session.cs | Physics steps once per LES simulation tick, including catch-up ticks, before serialization; delayed-update clock parity checks and connection cleanup. |
| octaryn-server/Source/Managed/Physics/BepuWorld.cs | Simulation time advances with actual steps; state published afterward; owner identity prevents reused public IDs from inheriting stale bodies. |
| artifacts/network-audit/MOTION-FIX.md | Historical explanation and experiment evidence. Read the qualifications below before reusing its claims. |

## Port invariants, then validate

- Use one authoritative simulation clock. Physics, replication, and snapshots
  must agree about which tick produced a state, including catch-up updates.
- Present source-time snapshots at 1x elapsed time. Do not chase packet arrival
  jitter with recurring render speed changes or interpolate the same state twice.
- Bound pending network work and handle disconnect/reconnect ownership and
  pooled buffers completely.
- Reproduce delayed-tick, loss/jitter, duplicate/stale snapshot, contact/reversal,
  underrun/refill, reconnect, and body-removal cases in the restored architecture.
  Profile sample-time velocity error, frame timing, corrections, queue age, and
  hold/refill events. Average FPS alone cannot prove smooth motion.

## Limits and historical mismatches

The preserved BuildRenderBodies path assigns the maximum body timestamp to the
whole body list. Only reuse this if replication guarantees a coherent shared
tick. Otherwise use per-body timestamps or an explicit coherent snapshot tick.

The 500 ms presentation buffer, 120-unit teleport threshold, 128-packet/256 KiB
pending queue, and 500 ms stale-packet deadline are demo tuning choices. A large
buffer adds presentation delay. Preserve configurability and measure suitable
values for the actual game rather than treating these as universal defaults.

MOTION-FIX.md says observer traffic bypasses artificial impairment. The final
preserved Session.Connections.cs enables simulated server-to-client unreliable
traffic for observers too; the observer client bypasses simulated uplink
impairment. Historical localhost results therefore do not verify the exact final
source configuration, and do not verify the restored Jolt engine.

BEPU body handles, contacts, integrator callbacks, solver settings, axes, and
ownership assumptions cannot simply be copied into Jolt. Reimplement the clock
and lifecycle invariants against Jolt's actual stepping API, then verify Jolt
collision/rest/removal behavior. No networking fix has been transplanted into the
restored engine as part of the directory restoration.

## Source-time movement metadata correction — 2026-09-13

The restored PoseHistory sample interpolated position, orientation and source
time while copying velocity, contact/flight flags and tick identity from the
future bracket endpoint. The newly integrated player consumed these fields for
clip selection, so jump/landing/reversal presentation could change early.
PoseHistory now interpolates velocity at the playback cursor and retains the
earlier discrete state until the next snapshot's actual double-precision time.
Velocity remains an interpolated animation input, not the derivative of the
bounded Hermite position or an input to client physics integration.

Position interpolation, camera/targeting inputs, 1x cursor advance, hold/refill,
duplicate rejection and teleport reset are unchanged. The server continues to
publish tick/time/position/velocity/contact together after its simulation step.
This deliberate PoseHistory edit supersedes the earlier nine-file byte-identity
claim for LocalSession; the preserved pre-RHI backup remains untouched.

The canonical octaryn_validate_client_player_model target now executes
PoseHistoryProbe.cpp against the production header. It passes grounded/jump,
landing, flight entry/exit, reversal, immediately-before-boundary rounding,
exact/held timestamps, refill, stale rejection and teleport cases. Evidence:
build/presentation-timing-build.log (pose_metadata=passed). These are CPU
source-time regressions, not new live network impairment or moving-camera GPU
measurements. The running client was not restarted during this correction.

## Rapid center reversal residency correction

WorldStream previously pruned its completed-column revisions only when the
worker observed a new center. The renderer evicts edge columns immediately.
If the main thread requests A, then B, then A while the worker is busy, an
A-only column can be removed from GPU residency but still marked complete by
the worker. Its unchanged revision would then never be delivered again.

StreamResidency.h now owns the CPU window/completion/delivery state. Every
WorldStream request invalidates out-of-window completion metadata under the
existing mutex, so the intermediate B window is observed even when generation
is busy. This is bounded to the supported radius-four window. Large queued and
query-column payloads are still pruned by the worker; the frame thread only
invalidates small revision records. Existing wanted checks hide out-of-window
queries/deliveries and reject an in-flight completion that is no longer wanted.
A completion whose coordinate is wanted again after reversal remains useful.
Edit-content revisions, queue bound, source clock and server protocol are intact.

The canonical player probe now includes StreamResidencyProbe.cpp, which calls
the same production transition methods with worker maintenance deliberately
absent between A/B/A requests. It covers delivered/queued edges, overlapping
residents, payload retention, in-flight completion, revision change, shrink/
regrow and bounds. A scratch copy omitting only request-time invalidation fails
the regression; the unchanged probe against production passes. Evidence:
logs/client/stream-residency-red.log and stream-residency-green.log.

The real asynchronous WorldStream probe also passes parser/edit/delivery checks
and 24,480 client/server terrain-parity samples in
logs/client/stream-residency-world-probe.log. Native regression build passes in
build/stream-residency-build.log. This proves CPU residency eligibility and
async delivery behavior; no new GPU traversal/capture or moving-world FPS
measurement was performed alongside the running user session.

## Terrain query and renderer delivery coherence

WorldStream previously published a newly generated immutable query column from
the worker before the ready queue delivered its matching render payload. Camera
collision and block targeting could therefore read a queued revision while its
predecessor was still drawn. The worker could also change that query view during
one frame's targeting/camera calls. An immediate away/back center change could
reveal retained query data after the renderer had already evicted that column.

StreamResidency now keeps each ready payload paired with its exact immutable
query copy. WorldStream::poll publishes that pair at delivery; later worker
completions only enqueue their own pairs. OpenWorld then completes the matching
GPU replacement before interaction and camera queries, with upload failure
exiting the frame. The public header records this ordering requirement. Window
exit clears a small visibility flag synchronously, so retained old data stays
unqueryable after reversal until the column is delivered again.

The two-entry ready queue and two retirement slots are bounded. Replaced query
owners and unwanted ready payloads are released by worker maintenance; full
retirement slots apply delivery backpressure until the worker drains them.
There is no additional block-vector copy. The existing one immutable worker
copy remains. Server authority, edit acceptance, snapshot schema, content-hash
revision identity and source-time interpolation are unchanged.

The production StreamResidencyProbe covers exact queued revision/pointer pairing,
move identity, unseen generated terrain, two queued revisions, worker-only query
retirement, backpressure/recovery, unwanted payload retention and the existing
residency reversal/shrink/regrow cases. It passes unchanged production source;
restoring only early query publication in a scratch header makes the same probe
fail with `generated but undelivered terrain must not be queryable`. Evidence:
logs/client/query-delivery-green.log and query-delivery-red.log.

The real threaded ClientWorldStreamProbe additionally withholds delivery through
initial generation, authoritative solid/air/re-add snapshots sharing an intent
epoch, and away/back residency. Queries remain on the last delivered payload or
unavailable; delivery immediately publishes the matching data. It passes along
with all 24,480 generation-parity samples, parser and edit checks in
logs/client/query-delivery-world-probe.log. These are actual CPU worker/file
tests, not a new moving-world GPU capture or measured FPS improvement.

Canonical validation is now available as
`tools/build/windows.ps1 -Action build -Target octaryn_validate_client_world_stream`.
It derives the Windows DLL search paths from the actual native dependency targets;
no caller PATH setup is needed. Final native/player/stream qualification passes
in build/query-delivery-reviewed-build.log. Packaging reaches a fully validated
stage but cannot replace the running game's directory; the repair applies only
after that coherent bundle is installed and the game restarts.

## Authoritative changes publish without a new client command — 2026-09-13

The process stream previously skipped an unchanged window whenever no client
block command was submitted that iteration. A module-originated world edit could
therefore remain absent from the client stream until another command or window
request. This was a publication gap, independent of client interpolation.

ModuleActivator.BlockRevision now advances only through the existing persistence
notification for actual authoritative changes. Queued, rejected and unchanged
edits do not advance it. The per-instance ChunkPublicationTracker tracks the
successful process-stream destination/window and full-snapshot revision.
ChunkStreamProcessBridge delegates publication after its existing authority tick
to ChunkStreamProcessPublication.PublishSnapshot. That method uses the native
window/write planner without executing a second simulation tick.

Every process publication is a complete, self-contained snapshot of overrides
in its requested window, even if metadata-only output was requested. Normal
LocalSession already requests metadata_only=0. The process binary format has no
delta/metadata flag, and the client replaces each column's edit list; omitting
edits from preserved columns would erase their visible overrides. The generic
native metadata snapshot utilities remain unchanged. No wire-format or edit-only
persistence representation change was required.

An initial destination requires a full baseline. Changed revisions and epoch-only
requests bypass the unchanged-window shortcut. The managed publication decision
also forces the native write planner, which otherwise ignores epoch changes when
coordinates/radius and the previous window match. This force does not claim new
client edits: command_delta records submitted commands separately from
publication_requested. Unchanged acknowledged requests still skip output.

The watermark advances only after the native writer returns successfully, using
the revision captured before the write. Failed writes remain retryable; the
tracker never acknowledges a changed revision through a metadata-only write.
Destination switches require a fresh full baseline, and a new ModuleActivator
cannot inherit the previous instance's watermark. Other snapshot APIs, including
RequestChunkColumns, do not acknowledge this process watermark.

The existing Octaryn.ServerWorldBlocksProbe now exercises the production tracker
and actual publication method. It covers changed-only revision increments,
initial/metadata-only handling, captured-revision acknowledgement, retries,
window/path/instance changes, and an actual module edit published to the same
window with no new client command. Its native JSON/binary fixture forces a real
binary-output failure with a nonempty isolated directory, verifies no
acknowledgement, then verifies a successful retry. It also checks that publication
does not advance simulation time and that an unchanged call skips native output.
The reviewed fixture additionally requests a new epoch at the same center/radius
with HasPreviousWindow=1, verifies the updated JSON and binary epoch and exact
retained override record, then verifies the next unchanged call skips writing.
A moved-window case verifies that an edited column in the overlapping region
retains that same override in both JSON and binary output.

Evidence: logs/server/fluid-publication-probe.log reports
chunk_process_publication=passed and chunk_publication=passed, including
epoch_only=passed, retained_overrides=passed and moved_window=passed. The underlying
canonical managed probe passed directly with the target's environment (exit 0).
The CMake aggregate target itself stopped at the existing live client-bundle
WinError 5 lock before reaching the probe; this is not a successful aggregate
publication claim. The same log reports fluid_rules_provider=passed, checks=162,
active_scheduler=false. No game process, GPU run or live fluid simulation was
used to validate this publication fix.

At that publication checkpoint, fluid scheduling remained inactive and
BlockChangeQueue was still unbounded. The process-file stream read BlockStore
without consuming that separate replication queue. The following qualification
supersedes that queue limitation. Persistence still snapshots all overrides on
each dirty save; its cost under repeated fluid changes remains unqualified.

## Bounded delta replication and process-only publication — 2026-09-13

ModuleActivator now selects an immutable BlockPublicationMode at construction.
Existing constructors default to ReplicationDeltas, including the in-process
HostExports authority. Standalone Host explicitly selects ProcessSnapshots for
both live and one-shot execution, including moduleless startup. Unknown mode
values fail explicitly; environment changes cannot switch an existing authority.

ProcessSnapshots does not allocate or enqueue an unused block-delta queue. Both
module edits and queued client edits still use the authoritative edit service,
persistence callbacks and BlockRevision. PendingBlockChangeCount remains zero;
delta-drain requests return the explicit unsupported result -2 without changing
the caller's header. Full process snapshots retry from authoritative BlockStore,
so no delta acknowledgement or regional queue-clearing policy is needed. Edits
outside the current window remain authoritative and persisted, then appear in
the complete snapshot when that region is requested.

ReplicationDeltas now uses an 8,192-record ring. The native edit owner plans the
exact primary/support changes and admits their required capacity before any
world mutation. A full queue defers the front queued client command without
popping it or advancing revision/persistence; retry after draining preserves
FIFO order. Direct edit application also checks capacity before commit. Existing
all-or-nothing snapshot drain semantics remain intact: an undersized output
buffer does not consume records. Successful regional process snapshots never
clear another consumer's replication history.

Managed evidence in logs/server/replication-backpressure-probe.log passes exit 0.
The actual process-mode fixture applies 8,194 module edits in one tick/save plus
one queued client edit, with delta_pending=0. It verifies native binary-write
failure/retry, an out-of-window edit retained until a later window move,
persistence reopen, explicit unsupported drain and the unchanged default delta
path. The actual default-mode authority saturation test reports
replication_backpressure=passed, capacity=8192, fifo=passed, revision=passed and
persistence_retry=passed. Existing epoch/override publication checks also pass.

Native evidence in build/replication-backpressure-native.log reports
block_backpressure=passed with 32,827 checks and capacity=8192. The separate fluid
evaluator passes 424 checks with evaluator_only=1 and active_tick=0. These are
native/managed qualification results, not proof of installation into the running
game, a GPU run or an active fluid scheduler. Bundle staging and installation
are recorded separately.

## Active authoritative fluid ticks — 2026-09-13

The fluid scheduler is now integrated into ModuleActivator through the optional
IFluidRulesProvider and the native FluidSimulation owner. This supersedes the
inactive-scheduler qualification above. ClientBlockCommandQueue and
BlockCommandSink forward the exact authoritative result.Changes lists, including
support cascades, to wake the scheduler. Native fluid application uses the same
authority, support and bounded replication checks; its complete changed count
marks persistence dirty and advances BlockRevision once per changed fluid tick.
Command changes retain their existing once-per-result notification. Saving remains
once at the end of the authority tick, after module commands and fluid work.

ChunkStreamProcessBridge sets the validated client intent's region before the
tick; radius zero is a valid configured region. Until a region is explicitly
configured, fluids neither advance nor receive change notifications. Repeating
the same region does not reset scheduling. Normal Tick and TickHostOnly each
advance fluids once, using simulation elapsed time clamped to 0.25 seconds;
zero elapsed time can service already-due bounded work without aging future
events. The default in-process ABI still needs an explicit region integration:
constructing an authority or requesting a generic snapshot alone does not enable
fluid simulation. ProcessSnapshots mode continues to avoid unused delta records.

Autonomous fluid changes feed the existing revision-based, complete process
publication path. A new client command is unnecessary; publication acknowledges
only a successful full snapshot, and persistence retains changes outside the
current stream window. No delta protocol or client authority was introduced.

Evidence: build/fluid-scheduler-native.log passes 17,099 fluid checks, including
250/500 ms timing, bounded work, retry, region repair, native application and ABI
cascades, alongside 32,827 replication-backpressure checks. The managed
logs/server/fluid-scheduler-probe.log reports fluid_simulation=passed for falling
water/lava timing, actual native application, save/reopen and autonomous process
publication. Its basegame_fluid_profile=passed uses the actual ModuleRegistration,
generated terrain and an 81-column region: 120 CPU samples, authority tick mean
2.253 ms / p95 5.414 ms / max 5.952 ms, and fluid-step mean 1.931 ms / p95 2.005 ms.
These isolated measurements include rendering=none and are not gameplay FPS.

This is source and staged qualification pending live installation and restart.
No new GPU or running-game validation is claimed by this fluid integration.
