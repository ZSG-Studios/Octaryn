# Authoritative fluid simulation recovery

Current status, 2026-09-14: authoritative simulation is connected to the server
tick. A reproduced outward-spreading failure after a terrain edit is repaired in
the release checkout, with native evaluator/scheduler/apply/C ABI verification.
The Windows package is rebuilt; graphical live qualification remains separate. Fluid
rendering remains on standalone Slang RHI and was not changed by this repair.

## Outward flow after terrain edits — 2026-09-14

The concrete failure was a water or lava source beside a floor opening. It first
flowed toward that drop as intended. After the opening was filled with stone,
the authoritative surface remained a one-direction stream; other directions
stayed empty and the pending queue reached zero. New sources on an already-flat
floor did spread symmetrically, so the evaluator's four directions were intact.

The scheduler woke only the edited block and six immediate neighbors. The
original [slope evaluator](https://github.com/ZSG-Studios/Octaryn/blob/3557cbfdc803ec034122bb55070b62b3b43b5588/references/old-architecture/source/world/edit/water.cpp)
looks four steps ahead for water and two for lava, after the candidate neighbor.
An unchanged nearby flow block therefore did not propagate a notification back
to every donor whose preferred route had changed.

Terrain/source changes now also wake the bounded dependency footprint: a
six-block Manhattan radius at the changed height and one block above it, at most
170 positions before deduplication. Direct neighbors take priority over this
wider work. Both due queues share the existing 8,192-position capacity and tick
budgets; ordinary flow-level continuations retain their local neighborhood.
At capacity, new direct work retires the latest slope dependency to cyclic
repair instead of being rejected behind lower-priority work. Interrupted slope
proposals that retry become direct work, preserving the existing retry policy.
External edit notifications contain only the resulting ID, so their dependency
invalidation is conservative. Water/lava delays remain 250/500 ms. Nearest-drop
selection, finite fluid levels, source creation, contacts and authoritative
apply/persistence rules are unchanged.

The native suite passes **35,529 checks**, including saturation with wider
dependencies followed by successful admission/service of a new direct update.
The new outward-flow regression fails against the
original scheduler with `plugged drop must resume symmetric bounded outward
flow`, then passes with the repair. It checks both fluids and four rotations,
two-block and maximum-lookahead drops, diagonal spread, the complete finite flat
footprint, stable termination, source-removal drainage and falling-to-floor
spread. A production C ABI fixture sends actual block-store edits using the
same resulting-ID-only notification contract, then checks all outward levels
and that only 113 fluid overrides remain above the generated floor. Native
snapshot/load preserves those overrides; this is not an on-disk save test.

Windows/MSVC evidence is under
`build/release-windows/tools/validation/fluid-audit/{before,final}.log`. These
are direct native production-source runs. The rebuilt Windows release subsequently
passes the canonical `octaryn_validate_cpu` aggregate in
`logs/tools/windows-cpu-complete.log`, including the same 35,529 native checks,
actual basegame fluid configuration, water/lava timing, publication, persistence,
and managed process-snapshot/backpressure fixtures. Its generated-world fluid
fixture recorded zero fluid budget stops with maximum pending work 171.
This is not a graphical packaged-client test. No shader, graphics backend,
generated ocean storage or active user save
was modified. Existing unnotified fluids still depend on cyclic repair; restart
and moving-region discovery latency remain separate work.

Evaluator preparation now exists in `World/Blocks/Fluids/` under the server.
The optional shared `IFluidRulesProvider` and basegame catalog provider expose
the content configuration, copied and validated into the native service. The
evaluator remains a pure proposal function; FluidScheduler and FluidSimulation
now connect bounded work to native authoritative edits, changed revisions,
persistence and publication. Its caller must supply a
stable read view for the full evaluation, including slope lookahead. Unlike the
old sampling code, which ignored failed adjacent reads and left them as air, the
new evaluator defers when required in-range data is unavailable. This deliberate
availability change avoids invented boundary fluid behavior. Original evaluator
parity applies to complete stable samples. Outside the signed vertical world,
samples remain known empty, and fluid cannot fall below the bottom world bound.

## Integrated scheduler qualification

The canonical native fluid target now passes 17099 checks, including the original
evaluator/apply fixtures, deterministic scheduler cases and C ABI ownership,
configuration, time and cascade/backpressure tests. The block-store target still
passes its 32827 backpressure checks. Evidence: build/fluid-scheduler-native.log.

logs/server/fluid-scheduler-probe.log passes actual ModuleActivator water/lava
falling, correct catalog levels through native marshaling/application, changed
revision, native process publication without a new client command, and save
reload. An actual basegame registration over generated terrain also applies flow
in an 81-column region. No simulation runs before a region is configured.

The generated-world profile runs 150 authority ticks, discards 30 warmup samples,
and measures 120 with console output suppressed: authority mean 2.253 ms, p95
5.414 ms, max 5.952 ms; fluid-step mean 1.931 ms, p95 2.005 ms. Nine simulated
changes occurred, maximum pending work was seven, maximum reads per tick 4180,
and 81 ticks reached a work/time budget. These are headless server measurements
including authority/save work, not client FPS or a broad fluid workload benchmark.
FluidReport and FluidStepMilliseconds provide per-tick profiling; changed-tick
logs include the fluid step duration.

Subsequent bounded terrain-column caching improves the same generated-world
fixture to fluid-step mean/p95 0.688/1.175 ms and whole authority mean/p95
1.015/4.036 ms, with zero fluid budget stops. Current evidence is
logs/server/terrain-cache-probe.log; terrain-sampling.md distinguishes these
time-budgeted measurements from fixed-work sampling benchmarks. The latest native
fluid suite passes 17101 checks in build/terrain-cache-native.log.

Pending positions are capped at 8192. Direct work is serviced before wider slope
dependencies, with earliest deadline then coordinates within each queue.
Event/continuation delays retain water 250 ms and lava 500 ms;
qualifying stale-fluid repair wakes immediately, as the original did. Thus those
delays are not unconditional minimums when repair discovers the same cell.
Stable enclosed sources use the original repair predicate and do not wake work.
Each step caps evaluations at 256, apply attempts at 128, reads at 65536, repair
samples at 4096 and repair neighborhoods at 64. The 2 ms time budget is checked
between units, not a hard maximum on a single evaluation or callback.

The active square uses signed world Y and a read-only one-column sampling halo
resolved from generated terrain plus overrides. Region movement retires pending
positions outside the active square and restarts the cyclic repair scan. Unknown
samples retry once, then retire to cyclic repair so permanent unavailability
cannot monopolize all pending slots. Applied-change backpressure and budget
interruptions retain work. Saturated notifications are recovered by the bounded
repair scan; recovery latency grows with region volume and still needs large
moving-region gameplay qualification.

Optional IFluidRulesProvider modules enable the service. Standalone process hosts
configure its region from validated view intent before ticking. ABI hosts retain
delta replication but need an explicit simulation-region owner; no region means
no fluid work. Every actual command change wakes its complete change list. Native
fluid changes use the same generated/override/support policy, schedule their
affected positions and mark revision/persistence once per changed fluid batch.

## Earlier evaluator preparation evidence

`octaryn_validate_server_fluid` now passes 424 native checks in
`logs/server/fluid-evaluator-validation.log`. Coverage includes every supplied
fluid level, falling/spreading/drainage, six-sided lava/water contact, different
water/lava slope distances, the three-source downward-flow exception, supported
water-source creation, vegetation, nonsequential remapped IDs, unavailable
samples and signed coordinate/vertical bounds. The native apply seam additionally
passes explicit-air override retention, restoring generated values, support
cascades, replication drain retention and rejection of client-only attempts to
place simulation levels. Snapshot/load checks here are in-memory store checks,
not new on-disk fluid simulation or scheduler tests.

Representative flat single-donor fixtures make 922 water reads and 114 lava
reads. Both are bounded below 4096 in these fixtures. This is a read-count check,
not an end-to-end simulation performance result; generated-terrain sampling and
scheduling costs still require qualification before enabling continuous work.

The actual basegame provider passes 162 managed catalog/validation/ownership
checks. The same server world-blocks probe passes the new actual process snapshot
writer test: module-originated changes publish without another client command,
failed native binary writes remain retryable, and publication does not run an
extra simulation tick. Epoch-only refreshes and moved windows preserve the exact
edited block in both JSON and binary output. Each process publication is now
complete: its binary format has no delta marker, and the client replaces its
override set. Requested metadata-only output must therefore retain all current
window overrides. Unchanged live calls still skip writes. Evidence is
`logs/server/fluid-publication-probe.log`.
The underlying probe ran directly with the canonical target's environment and
exited zero. Its CMake aggregate stopped earlier at the live client bundle lock;
`build/fluid-publication-managed.log` is not a passing aggregate-test claim.

## What exists and what is missing

`octaryn-client/Shaders/Voxel/WorldFluidMesh.slang` computes corner heights and
surface flow attributes from supplied block IDs/levels. `WorldFluidShade.slang`
and atlas animation restore appearance. These GPU passes do not spread fluid,
remove unsupported flowing blocks, create sources, or change lava into stone.
The client can display authored fluid-level fixtures without those gameplay
rules being implemented. Existing GPU fixture evidence therefore does not
establish fluid simulation parity.

The original implementation is preserved read-only at commit
`3557cbfdc803ec034122bb55070b62b3b43b5588` under
`ref/upstream-octaryn/references/old-architecture/source/`.

| Original source, relative to that directory | Actual contract |
| --- | --- |
| `world/window/update.cpp:144`, `world/edit/queue.cpp:48–82` | Services queued edits and fluid work on world updates, including frames with no new user edit. |
| `world/edit/apply.cpp:118–120` | A changed edit touching fluid schedules its neighborhood. |
| `world/edit/water.cpp:22–27` | Water delay 250 ms; lava delay 500 ms; dispatch/apply limits 256/128; repair scan limits 4096 blocks and 64 schedules per service. |
| `world/edit/water.cpp:252–351` | Downward priority and horizontal donor selection toward the closest drop; search distance four for water, two for lava. |
| `world/edit/water.cpp:467–545` | Falling fluid, increasing horizontal levels, unsupported-flow removal, supported two-source water creation, and lava touching water becoming stone. Existing fluid sources otherwise persist. |
| `world/edit/water.cpp:587–629` | Incremental repair scan seeds stale loaded fluid, including behavior not initiated by a fresh edit. |
| `world/edit/water.cpp:683–729` | Deduplicated due positions, earliest deadline retained, scheduling center/above/below/four horizontal neighbors. |
| `world/edit/water.cpp:732–772` | Checks the expected current block before applying a computed result; persists successful changes and schedules affected neighbors. |
| `world/edit/water.cpp:790–910,964–991` | Bounded sample batches, worker computation, budgeted authoritative application, repair and rescheduling. |

The old scheduler's vector is not globally capacity-bounded. Its service limits
and worker organization are evidence to recover deliberately, not permission to
copy unbounded pending work or old global state into the active server.

## Active owners available for integration

- `octaryn-server/Source/Modules/ModuleActivator.cs:244–284` runs client command
  draining, authority/module ticks, one fluid step and persistence. Both tick
  variants service fluids after authority work. `Tick/AuthorityTickRunner.cs` exposes command drain,
  player simulation and world-time callbacks.
- `octaryn-basegame/Source/Module/GameContext.cs:23–28` currently records frame
  state; it does not evaluate fluids.
- `octaryn-server/Source/World/Blocks/Store/BlockEditService.cpp:16–34,113–121`
  already resolves generated terrain plus saved overrides and removes redundant
  overrides. Its `apply_block_edit` at lines 165–200 applies a requested edit and
  the unsupported block above it. Complete changes feed fluid wakeups at the
  authority owner; the native fluid adapter also schedules its own cascades.
- `octaryn-basegame/Source/Content/Blocks/BlockCatalog.cs:353–407` provides fluid
  kind/source/level queries and constructors. Current basegame water IDs are
  14–21 and lava IDs 31–38. These mappings belong to content; the generic server
  must consume a validated rules contract rather than hardcode these ranges.
- `octaryn-shared/Source/World/Blocks/IBlockAuthorityRules.cs` currently supplies
  known/placeable/solid/support rules. IFluidRulesProvider now supplies a separate
  validated fluid contract with original leaves/grass-dependent replacement rules.
- `World/Blocks/Commands/Queue/ClientBlockCommandQueue.cs` reports successful
  changes using the actual changed list, including support cascades, for fluid
  wakeups and persistence. `World/Blocks/Store/`
  already owns `BlockChangeQueue` and replication conversion.

## Original integration checkpoints (now connected; broader qualification remains)

1. Recover the original sample evaluator and slope/donor decisions as testable
   gameplay rules, with basegame catalog definitions supplied through an explicit
   contract. Keep generic queue/state handling in the server world/block owner;
   keep content mappings and rules in basegame. Split the original large file
   along those responsibilities instead of importing it wholesale.
2. Define bounded active simulation regions and pending work before connecting
   the scheduler. The original queried loaded chunks and Y 0–255; the active
   world uses sparse overrides over generated terrain and signed Y -256–255.
   Unknown/unavailable neighbors must not become invented air. Region changes,
   queue saturation and unload/re-entry need explicit retry or repair behavior.
3. Schedule from successful authoritative changes, then service due work in the
   authoritative write phase with count/time budgets. Preserve the original
   delays using simulation elapsed time, independently of the day-speed control.
   If computation runs on workers, pass immutable samples and revalidate results
   before applying them. Do not mutate the block store from the client renderer.
4. Apply successful results through the existing generated-plus-override policy,
   publish every resulting change through replication, and mark persistence
   dirty. Simulation must not be restricted to client-placeable source blocks:
   intermediate flowing levels and contact products are authoritative results.
5. Qualify an edit-triggered slice first, then restore bounded repair/residency
   seeding for existing fluids. An edit-only service is useful but does not yet
   reproduce the original loaded-world repair behavior.

Replication backpressure is now qualified. `BlockChangeQueue` retains a fixed
8192-entry FIFO and still drains only when the caller supplies space for the
entire pending set. Native edit application plans the exact primary/support
changes, admits the whole plan, then commits through the existing override
policy. Capacity deferral makes no mutations. Queued client commands retain the
blocked front; direct module calls return false before mutation. Invalid and
unchanged proposals need no capacity. This assumes the existing single authority
thread and stable policy/read view; it does not add out-of-memory rollback.

Standalone `Host` explicitly selects immutable `ProcessSnapshots` delivery at
construction, including one-shot and moduleless operation. It never allocates or
passes an unused delta queue; the block store, changed revisions, persistence and
retryable complete process snapshots remain authoritative. Default/HostExports
instances retain `ReplicationDeltas`. A regional process snapshot never clears a
global event queue, and process-only authorities explicitly reject delta drains.

The native block-store probe now runs on Windows rather than reporting a skip.
Its 32827 backpressure checks and the existing 424 fluid checks pass in
`build/replication-backpressure-native.log`. The managed probe passes actual
8192-event saturation, unchanged revision/persistence while deferred, exact FIFO
retry and on-disk reload. Its process-mode case applies 8194 module edits plus
one queued client edit with zero pending deltas, forces native publication failure
and retry, and verifies a distant edit in a later window and after persistence
reload. Evidence: `logs/server/replication-backpressure-probe.log`. These tests
do not activate the fluid scheduler or demonstrate live fluid gameplay.

The internal native `apply_block_edit` already accepts known flowing-level IDs;
client placeability is a separate command-queue check. Reuse the internal apply
path for proposals to preserve generated-block fallback, explicit air overrides,
restoring-generated-value cleanup, and support cascades. Do not relax client
command validation or write directly to the block store as a shortcut.
Scheduled fluid work must retain deferred proposals and retry them; it must not
consume a work item merely because evaluation completed. Bounded pending work,
native configuration marshaling, region repair and time budgets are now connected
and covered by the qualification above. Large-region repair latency, sustained
fluid-save cost and combined live rendering remain further qualification work.

CPU tests should cover falling columns, flat spreading, nearest-drop choices,
source removal and drainage, supported two-source water creation, water/lava
contact, replaceable vegetation, signed chunk boundaries, stale results, timing
and queue limits. Server fixtures must additionally prove persisted/replicated
block changes and reload behavior. Rendering tests remain separate: visible
surface movement alone cannot prove authoritative simulation or persistence.

The evaluator, native configuration, scheduler and authoritative apply/publish/save
paths are integrated and qualified in isolated production-owner fixtures. Full
fluid gameplay parity remains unfinished until the new package is installed and
combined live behavior and sustained workloads are verified.
