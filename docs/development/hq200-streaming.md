# HQ200 streaming implementation checkpoint

This is source and CPU-fixture evidence. It does not establish a 0.2 ms CPU
streaming cost, 0.2 ms GPU streaming cost, or a 5 ms frame. Canonical build and
separate DX12/Vulkan runtime qualification remain required.

## Allocation and publication

`MapAssetAllocation.cpp` binds existing shared textures on the owner, then starts
one exclusive background allocation job for the active tile builder. It creates
unpublished vertex/index/material/indirect/meshlet buffers and new texture views.
The worker does not modify shared pool membership, sampler caches, command
encoders, or published maps. The owner polls completion without waiting.

When RT is required, `MapRayResources.cpp` also creates BLAS, TLAS, build scratch,
instance storage and the compact-size query pool in that job. Material geometry
order, per-primitive index offsets and opaque/alpha-test flags are unchanged.
Build recording and submission remain on the renderer owner. The compacted BLAS
allocation has its own exclusive asynchronous result after the first GPU fence;
the owner records compaction only after that allocation completes. Publication
still requires the final copy/TLAS fence and prepared collision. Monolithic and
item startup retain synchronous initialization, outside this streaming path.

The owner prepares one material record before checking elapsed time again. Its
five descriptor lookups and any new sampler creation are still indivisible
driver work. Pipeline creation remains prewarmed during tiled-world startup.

Uploads are capped at 2 MiB per pump, with individual buffer and block-row texture
copies capped at 256 KiB. Texture staging charges backend row pitch and 512-byte
alignment. The default owner pump target is 0.2 ms; it is a **soft** limit checked
between operations. GPU builds, command allocation, barriers and submission are
not preemptible through this API and still require measured cost limits.

Cancellation is checked before every worker resource call and between image and
primitive loads. Cancelled upload resources remain retained until the last data
fence completes; incomplete textures are removed from shared-cache membership.
Unwanted tiles skip new ray builds. Retirement separately polls the map's own ray
fence and compaction allocation so normal retirement does not wait on either.
Shutdown can still wait for outstanding driver allocation calls.

## Work and memory bounds

The tile cooker now defaults to 16,384 triangles per tile without simplification.
The `octaryn_map_tiles` target cooks and checks geometry/UV conservation. Bundle
packaging now depends on it and stages `Maps/hq200.json`; ordinary HQ200 startup
selects this manifest. An explicit map-manifest override still takes precedence.
`Maps/map.json` and its monolithic payload remain the reference. HQ200 tiles share
the reference's cooked texture directory, avoiding a second 1.3 GB payload copy.
The old 137-tile Bistro evidence remains historical evidence for its old content,
not qualification of this new partition.

HQ200 tile preparation limits are checked before owned payload allocation:

| Payload/work | Limit |
|---|---:|
| Input GLB and declared buffer bytes | 16 MiB |
| Aggregate encoded images | 64 MiB |
| Accessor elements / triangle indices | 49,152 |
| Triangles / material primitives | 16,384 / 2,048 |
| Retained unique cooked texture mips | 96 MiB |
| Concurrent loading/prepared/uploading slots | 2 |
| Per-slot preparation reservation | 256 MiB |
| Total preparation reservation | 512 MiB |

HQ200 requires cooked textures and does not decode a missing image cache. DDS
metadata checks the remaining payload budget before mip vectors are allocated.
Bounded tiles do not request external geometry buffers from the parser. External
image reads remain bounded and exact. Other profiles retain the reference loader
limits. These are owner payload and work reservations, **not a proof that total
process RSS, parser allocator overhead, or driver memory is below 512 MiB**.

Tile GPU admission retains the explicit tile working-set budget and also checks
the observed OS process-local video-memory usage against 70% of its DXGI budget.
It includes the candidate's queried AS peak, geometry, unique textures and other
pending reservations. Admissions since the same cached OS sample are charged;
they cannot all reuse the same apparent free bytes. Unknown OS budget coverage is
reported as explicit-budget-only, not as passing the 70% contract. Other renderer
allocations still need their own admission policy, and external pressure can
change after a sample. Pressure pauses new tile admissions while existing
collision/resident ownership is retained.

## Timings and deadlines

Logs distinguish owner setup, background resource allocation, ray allocation,
compaction allocation, owner ray pumping, upload copies and material preparation.
`tile_published` includes elapsed time since the tile became wanted. A 2,000 ms
readiness deadline reports a miss and unready collision; it never publishes
partial data to satisfy the deadline. This is diagnostic policy, not a measured
prefetch-distance guarantee. Velocity-based lookahead and calibrated readiness
percentiles remain open.

## Focused evidence

- `logs/build/hq200-map-tiles.log`: native source/encoded/accessor bounds; valid
  bounded tile loading; material/UV/image fidelity; 16,385 input triangles split
  into two <=16,384 units with unchanged positions and winding.
- `logs/build/hq200-map-hash.log`: both CNG and portable SHA paths, 14 known-vector
  sizes, five cache keys, two existing cached assets, exact rewritten digests,
  corruption rejection and preallocation DDS payload-budget rejection.
- `logs/build/hq200-tile-budget.log`: 8,283,009 small OS-budget boundary cases,
  integer-overflow cases, upload alignment/chunk limits, and real ray-input
  construction preserving material order, alpha flags and retained counts.
- `logs/build/hq200-asset-syntax.log`: focused native syntax checks. No GPU result
  is inferred from these checks.

The new full Bistro cook passed CPU validation: 299 tiles, all 4,146,017 triangles,
maximum 16,384 per tile, 270 unique encoded images and 272 cooked texture variants.
Every tile passed the production HQ200 preparation path without source decoding;
maximum retained prepared payload was 96,787,816 bytes. Full-source conservation
checks oriented positions, UVs, colors and tangent handedness using two 64-bit
digests. The material/UV/sampler fixture is separate; these checks do not establish
full-scene image equivalence. `verify.json` binds every tile SHA256 to this result.

`StageMapTiles.py` validated and copied the complete cook to a separate staging
fixture without changing the current canonical bundle. Ten packaging tests reject
changed source/payloads, incomplete verification coverage, escaped paths, corrupt
LOD/textures, invalid bounds and count mismatches. Evidence is in
`logs/build/hq200-bistro-{cook,verification,stage}.log` (cook stages are numbered),
`hq200-bistro-assets.json` and `hq200-tile-stage-tests.log`. The native source hash
is `55cbe3aea10abc2b3857184dcf6a7f63285e4445bb16cea146ee453efe9d8f74`.

Next qualification needs canonical packaging, both backend allocation and
cancellation stress, matched initial/boundary/settled/moving timings, maximum
individual driver-call costs and repeated residency/memory checks. Existing
reference captures and limits must remain available for comparison.

## Next runtime workload and reporting

`capture_tile_world.py` now accepts `--performance-profile HQ200 --width 2560
--height 1440` with an explicit cooked manifest. This enables the actual profile,
including its 720p–1080p internal range, while recording the output dimensions,
build/assets, settings, CPU/GPU/lighting profiles and request-ready diagnostics.
`--uncapped-fps` removes diagnostic pacing; it does not disable the watchdog.
Use separate RHI-validation correctness runs and three uncapped repetitions per
backend without validation or concurrent builds/probes. Keep the 50 ms frame
watchdog and existing capture/residency checks; preserve a failure as evidence.

Use startup-only `--smoke --frames 600 --require-all-tiles` first. Then use
`--frames 3240 --route-warmup-frames 1200 --require-all-tiles
--require-residency-transitions` for two complete route cycles and a final hold.
The prepared `logs/tools/hq200-tile-qualification.json` records exact commands;
it is a proposed queue, not execution evidence. The explicit 4 GiB tile working
set is still subject to the independent observed 70% OS-budget admission limit.

The existing route makes 384 m cuts and traverses that distance in 240 frames,
including views outside Bistro. At 200 FPS that is approximately 320 m/s. It is
an **extreme renderer streaming test**, not the normal gameplay acceptance case;
authority remains stationary. The separate gameplay-route owner now has authored
phase/waypoint input through normal prediction and authority transport; it still
needs a compiled, in-bounds Bistro route and its own runtime measurement. Existing
nominal walk/sprint speeds are 5/9 m/s: walk fits an 8 m/s traversal envelope,
while sprint must be reported separately. Collision readiness and calibrated
velocity lookahead remain unqualified. Do not use fast frame-driven completion
as proof of prefetch safety.

`tile-performance.json` separates pre-authority-ready work from later work,
reports successful request-ready distributions alongside deadline/cancellation/
pending counts, and reports all matched frame tails. Capture-overhead frames are
also reported separately; no other expensive frames are discarded. Individual
upload call maxima are cumulative per builder and are not treated as independent
percentile samples. Historical binaries omit zero-byte material-only pumps, so
their recorded pump tails are incomplete. Build24 includes the positive-CPU
zero-byte pump logging change; its pending 299-tile capture must retain these
records in the same pump distribution. Background allocation costs overlap
other work and must not be summed into frame-thread costs.

## Item and scene capacity prewarming

`WorldRayPrewarm.cpp` now queries and creates three reusable scene TLAS/record
capacities plus both fenced frame instance/scratch buffers in the exclusive
startup worker. The default reserves 1,000 items and 512 map records, covering
the 299-tile cook. `OCTARYN_CLIENT_ITEM_PREWARM_CAPACITY=10000` selects the separate
stress tier. The absolute item bound remains 10,000; exceeding the default warm
tier preserves rendering but reports capacity growth, which can still allocate
inline and is outside the normal prewarmed claim.

Rounded buffer sizes and queried AS sizes are checked against a 256 MiB explicit
prewarm bound and a fresh 70% OS-budget observation before creation. A shared
owner-lifetime reservation also charges the full logical capacity to later tile
admission independently of OS sample IDs. A second fresh observation checks
process-local usage plus outstanding capacity credits. This can conservatively
count placed resources twice when their heaps are already reflected in OS usage.
Unavailable OS budget data is logged rather than treated
as a verified 70% limit. The budget check does not control external pressure or
make individual driver calls interruptible; total/max-call startup costs and
before/after usage are logged.

The pool retains capacity across map unload. Only a snapshot with no current or
in-flight frame owner can be rebuilt; completed-frame recycling clears geometry
owners. Retirement includes all pooled snapshots and releases them after the
queue-wide fence. No placeholder visible instances or modified material/BLAS
indices are introduced. Dynamic reflection bindings are unchanged.

The item-history hash table now uses `ItemHistoryMemory`, an owner-local bounded
free list behind the standard PMR interface. Two entity generations are warmed
because new IDs are inserted before absent prior IDs are erased. It retains at
most 20,008 allocation blocks, eight size classes and 8 MiB; the measured native
fixture retained 700,536 bytes for the normal tier and 2,284,536 for stress. The
existing raster-instance buffers and four immutable item assets remain shared.

`validate_scene_capacity.py` passed 4,096 exclusive-ring rotations, normal/stress
capacity arithmetic and overflow guards, and eight complete ID-generation
replacements with zero upstream history allocations after warming. Final logs
are `logs/build/hq200-scene-capacity-final.log` and the focused source syntax log.
The earlier standard PMR pool unexpectedly grew on the second generation; its
failed logs remain preserved. These CPU checks do not prove a GPU first-arrival
latency improvement. Canonical compilation, both backend validation, constant
allocation counters through 0→1,000 items and separately configured 10,000-item
stress, image checks and frame-tail measurements remain required.

### Build23 DX12 runtime checkpoint

`logs/client/hq200-build23-items1000/map-dx12-on-0-j8bh8zz8` passed the actual
HQ200 DX12 capture with RHI validation and was visually reviewed. It uses the
authority item-visual fixture's single map primitive, not the full Bistro scene.
Schema3 records
1,000 world items, 256 awake items and four module GLB assets at 2560×1440 output
(the last recorded internal extent is 1920×1080). This is a captured correctness
run, not an uncapped CPU/GPU performance qualification.

Startup reserved 2,433,120 logical GPU bytes across three snapshots and two
frame slots. Its worker cost was 30.369 ms, with an 11.501 ms maximum recorded
allocation call. At 117, 237 and 357 TLAS builds, the counters remained three
TLAS allocations and three snapshot-record allocations. Recorded TLAS GPU times
were 0.1019, 0.0999 and 0.1027 ms. Snapshot storage stayed at 1,646,688 bytes and
frame temporary storage at 786,432 bytes; no capacity-growth marker was present.
The history pool retained 700,536 CPU bytes, matching the production allocator
fixture. These observations confirm capacity reuse for this run; they do not
prove that every backend staging/descriptor operation is allocation-free.

This build23 checkpoint exposed an outstanding-reservation accounting ambiguity.
The forced OS samples increased from 1,262,514,176 to 1,262,600,192 bytes—only
86,016 bytes versus the 2,433,120-byte logical reservation. The DX12 backend uses
D3D12MA placed resources, so reuse of already-budgeted heaps can explain a small
OS delta; delayed observation is also not excluded. A new sample ID alone does
not prove attribution of the new bytes. Follow-up source now attaches one shared
`DeviceMemoryReservation` to every prewarmed snapshot/frame resource owner and
includes its full logical byte charge in subsequent tile admission. The credit
is not cleared by an OS refresh or pool-reference release while frame owners
remain. Its last owner releases it; resource members are destroyed before their
credit reference. The possible conservative double-charge here is 2,433,120 bytes
(2.32 MiB). No memory-limit violation is established by build23. The native
reservation fixture passes cached/fresh observation, shared-owner retention,
last-owner release, overflow rejection and admission boundaries; the existing
4,096-rotation scene/history fixture also passes. Eight affected translation
units pass focused syntax checks. Evidence is
`logs/build/hq200-scene-reservation.log` and
`logs/build/hq200-scene-reservation-syntax.log`. Canonical build24 passes;
do not attribute this follow-up correction to the build23 executable.

Vulkan item-capacity reuse, explicit 10,000-item GPU stress, and matched uncapped
first-arrival frame-tail measurements remain open.

## Build24 first 299-tile startup failure

The first 2560×1440 DX12 HQ200 tiled run failed at renderer frame 376 after
72/299 tiles were published (last successful readiness 7362.372 ms). Evidence is
`logs/client/hq200-build24-tile-qualification/dx12/startup/tiles-dx12-3mzeyxft`.
The stale `atlas_encoder` stage and overwritten tile error obscured the cause;
the later acquired-swapchain-image error arose during shutdown.

A GPU-free production preparation/collision replay reproduced failure on tile 176:
all 16,384 triangles lie below Box3D's existing 2.5e-7 m² area threshold; its largest
triangle is 1.22901766542e-9 m². This legitimately yields no supported collision
triangles, but the tile owner treated that as a failed build. Follow-up source
publishes an explicit valid-empty collision owner without creating a body. It
keeps tile readiness/membership while leaving all raster/RT geometry untouched.
Invalid counts, indices, non-finite data and genuine native build failures remain
errors; neither the physics threshold nor the rendered detail is reduced.

The native fixture passes empty/solid replacement, 100 empty-tile cycles, invalid
input rejection, exact threshold acceptance and preservation of an adjacent
raycastable floor (`logs/build/hq200-empty-collision-tiles.log`). The frame owner
now logs `tile_pump` and preserves its exact error. This source correction still
requires canonical compilation and a repeated 1440p tiled GPU capture; the
original failure is retained.

The complete packaged production replay now passes preparation, collision
publication and removal for all 299 tiles. Tiles 176 and 226 are the two valid
empty collision cases, each retaining all 16,384 rendered triangles. Evidence:
`logs/build/hq200-packaged-tiles-fixed-retry.log`. This CPU result does not qualify
GPU residency or readiness latency.

### Build25 DX12 full-residency retry

The unchanged 600-frame, 2560×1440 startup retry exited successfully with no
fatal frame or RHI error, but only 111/299 tiles were resident. Its missing capture
is a failed qualification, preserved at
`logs/client/hq200-build25-tile-startup/tiles-dx12-g4czew6a`.

A separate 2,400-frame run passed exit, RHI validation and the inspected capture
with all 299 tiles resident, zero preparation/uploads outstanding, 804,216,500
logical geometry/AS bytes and 1,319,998,448 shared texture bytes. Evidence is
`logs/client/hq200-build25-tile-residency/tiles-dx12-wjsosadq`. Final tile 298
reported 22,561.512 ms from its request to collision/ray-ready publication; first
full residency was recorded at tile-session frame 2678, and the settled capture
at renderer frame 1596. These frame counters have different origins.

**Initial playable startup against the 5/8-second budgets is unqualified.**
Authority/input became ready at SDL uptime 5,487.290 ms and session elapsed
1,197.888 ms while the complete wanted set was incomplete. That milestone does
not prove the required visible geometry and collision are ready. Conversely,
waiting for all 299 tiles does not establish when that smaller playable set was
ready. The final request duration is a 22.562-second lower bound for complete-map
residency, a separate workload. Neither exact process-launch-to-full-residency
nor initial required-visible/collision readiness was recorded; do not combine
clocks with different origins or infer a playable-startup failure from full-map
time. The longer diagnostic preserves the 50 ms frame watchdog and all content.
This historical result is DX12 full-residency correctness only; subsequent
backend and lifecycle results are recorded separately below.

### Explicit tiled codec comparison assets

`tools/validation/prepare_tiled_texture_variants.py` stages the existing safe BC7
cache beside the lossless cache without recooking or changing packaged defaults.
The manifests are `build/release-windows/client/map-variants-hq200/lossless/map.json`
and the adjacent `bc7/map.json`; `variants.json` records source, each GLB/image/LOD,
manifest and validated DDS payload identities. Both manifests contain identical
299-tile bounds, geometry paths and view settings. Their manifest SHA256 is
`56c08d40615f45919fce0db20522876c6a3ff925b59d1ea8a6c3fe6b89f571eb` and aggregate
geometry identity is `931fc4fdb99eec85ef86f86be3ae38d833a85c993843594944da2088495139de`.
Every geometry dependency also matches the currently packaged HQ200 source.

Lossless texture payload is 1,319,998,448 bytes; safe BC7 is 841,716,572 bytes.
Both have 272 content keys; only cooked texture payload/receipt identities differ.
The helper validates DDS digests and LOD source binding, preserves external image
URI dependencies, and rejects escaping paths or source/output overlap. Four
focused identity, corruption and isolation tests pass in
`logs/build/hq200-tiled-variants-tests.log`; staging evidence is
`logs/build/hq200-tiled-variants-stage.log`. Payloads are hardlinks where supported
and must not be edited in place. This prepares the comparison only: fresh quality
captures and three matched runs remain required before changing the HQ default.

`logs/tools/build27-tiled-codec-qualification.json` prepares two static native
2560×1440 quality processes followed by three alternating HQ200 loading pairs.
The tiled capture helper now supports `--fixed-sampling`, `--camera-origin` and
`--capture-ready-frame`: quality requires the exact common ready frame 3000,
all 299 tiles, native internal/output dimensions, fixed camera/light, ray phases
and 16.667 ms presentation delta. It rejects a later first-settled capture instead
of comparing unequal phases. Compare the two recorded `quality_signature`
objects as well as images. The focused rejection fixture passes in
`logs/build/hq200-tiled-quality-helper-tests.log`. These commands remain unrun;
the asset validation and per-capture identity hashing warm files, so cache state
is uncontrolled and no first process is labelled cold. This single view is a
narrow codec comparison, not the full continuous-motion/material quality gate.


### Fence-ready texture preparation reuse (build28 source)

Tile preparation now accepts an immutable weak index published by the pool owner
only after the upload fence completes. A worker locks a strong opaque ticket for
each requested content key; a hit carries the verified format, dimensions, mip
count, alpha classification and byte count without reading or allocating the DDS
payload again. Misses retain the original strict receipt/hash validation. Encoded
source-image reads and exact content-key hashing remain in this first slice.
No quality, load-time or memory improvement is claimed before matched runtime.

Snapshots are cached by ready-resource generation and exact cache-directory
namespace. Admission neither canonicalizes paths nor queries files; the existing
64 MiB source-size check moved into the worker. Different textual path aliases
may miss, but cannot borrow another namespace's texture. The world cache is
immutable for its lifetime; live disk mutation is not a supported cache-reload
mechanism. An existing hit uses previously verified GPU content, not a new check
of files that may have changed after publication.

Weak indexes cannot pin a retired world. Strong prepared tickets keep requested
resources accounted in the unique pool total through eviction, cancellation and
builder adoption; adoption acquires the map owner before dropping the ticket.
Pending/unverified resources never enter the index. Each newly fence-ready pool
generation republishes a snapshot; old worker snapshots remain immutable.
`OCTARYN_CLIENT_TILE_TEXTURE_REUSE=0` is an explicit diagnostic comparison path;
default is enabled. `capture_tile_world.py --texture-reuse on|off` records and
requires the exact startup activation marker. Existing owner `tile_stream` records
now include cumulative `texture_reuses` and `avoided_dds_bytes` from completed
asset preparations; no additional per-frame log call was added. Partial cancelled
preparations are not counted, and skipped payload bytes are not GPU memory saved.

The production preparation fixture verifies hits with no DDS on disk, exact alpha
promotion/deduplication, namespace misses, strict corrupt-cache rejection,
cancellation/failure cleanup, eviction and multiple-ticket lifetime. A separate
GPU-free native probe exercises the production pool: fence-ready/verified filter,
immutable generation reuse, distinct world namespaces, unique allocation charge,
builder ownership transfer, cancellation/eviction and map-switch isolation.
Evidence: `logs/build/hq200-texture-reuse-prepare.log`,
`hq200-texture-reuse-pool.log`, `hq200-texture-reuse-syntax.log` (six native owner
units), and `hq200-texture-reuse-evidence.log` (CLI activation rejection checks).

The same source checkpoint fixes staged resource default states: immutable
textures/SRV buffers finish in ShaderResource, raster/LOD indices in IndexBuffer,
and indirect arguments remain IndirectArgument. Upload destinations and explicit
final transitions are preserved. This matches the synchronous reference owners;
DX12/Vulkan runtime and paired images remain required for the staged correction.


### Bounded AS lifecycle pipeline (build29 source, runtime pending)

The first build28 repeat isolates a remaining queue after texture preparation:
lossless-off/on final request readiness was 21,388.275/20,357.915 ms; safe BC7-on
was 19,202.646 ms. Lossless-on skipped 9,447,332,436 DDS payload bytes, reducing
summed overlapping asset-worker CPU from 15,845.647 to 4,584.208 ms. Actual upload
bytes remained 1,745,204,792. Despite that reduction, 56 uploaded/ray-pending tiles
remained after preparation drained; the final sampled tail lasted 3,648.384 ms.
BC7-on had a 61-tile, 3,947.143 ms tail. These are single-repeat observations, not
three-run conclusions or process-launch/playable-startup times. Source evidence
is preserved in `logs/build/hq200-as-pipeline-before-build28.json`.

The previous owner advanced one tile's complete BLAS/compaction lifecycle per
pump, including wait-only calls. Lossless-on made 1,705 ray calls, 1,406 not-ready;
242 of 299 tiles needed six calls. Median publication spacing was 61.152 ms.
AS duration was not included in historical main-command GPU spans: independently
submitted build/copy lists executed before the deferred main command buffer.
Those GPU timings cannot establish the streaming-AS budget.

New source separates `poll_map_ray_scene` from `perform_map_ray_work`. Polling
observes a fence/future and publishes completed ownership; it does not allocate,
launch a task, submit commands or wait. The tile owner polls at most four active
lifecycles, then performs at most one explicitly classified operation: build
submission, compact-allocation start, or compact-copy/TLAS submission. Waiting
first tiles cannot consume the operation allowance. Default in-flight capacity
is four; `capture_tile_world.py --as-inflight 1` selects a serialized version of
this new scheduler for same-binary comparison, not the previous algorithm. Only
capacities 1 and 4 are accepted. Both retain the same one-operation quota and
soft 0.2 ms admission allowance. Driver-call duration remains uninterruptible and
actual overruns remain recorded. GPU callbacks bracket the independently
submitted commands, including production resolution feedback, with at most one
external AS span per rendered frame.

Peak-AS reservations survive cancellation into retirement, including the compact
destination still held in a future. Pending retired work retains lifecycle
capacity until the existing retirement pass observes its real completion.
The scheduler reuses that pass's pending count, avoiding another retired-fence
scan. Full frame CPU still includes the existing retirement scan; the narrower
`tile_ray_schedule` CPU duration does not. The single upload owner, 2 MiB aggregate
upload allowance, 256 KiB checks and two fixed CPU preparation reservations are
unchanged. This slice does not prove the entire 512 MiB retained-memory bound:
collision handles waiting in Ray still require separate accounting.

`tile_ray_schedule` records polls, fence/allocation waits, work classification,
actual submission count, in-flight capacity, publication/cancellation counts,
operation/owner CPU cost and cumulative counters. The report rejects inconsistent
caps/classification and retains soft-budget overruns. Native policy tests cover
capacity rejection, four polls, single work/submission, pending-first fairness,
and cancellation slots; Python tests cover activation and damaged evidence.
Logs are `logs/build/hq200-as-pipeline-{policy-tests,evidence-tests,report-tests,syntax}.log`.
The initial fixture-only unsigned initializer compile error is preserved in
`hq200-as-pipeline-policy-tests-initial-failure.log`. Actual DX12/Vulkan lifetime,
quality, cancellation, GPU query attribution and matched startup results remain
pending the canonical build and coordinated renderer runs.

### Vulkan private initialization repair (build30 source)

Build29's DX12 all-299-tile control passed, but its Vulkan counterpart failed at
frame31 with actual concurrent `vkQueueSubmit`, command-pool and command-buffer
writes. Failure evidence remains at
`logs/client/hq200-build29-as-smoke-vulkan/tiles-vulkan-yh4o797p/client.log`.
The allocation worker's `createTexture(nullptr)` still performed a backend
layout transition through one shared recording ring, followed by queue-idle;
normal queue submission and retirement used the same raw queue without shared
host synchronization. Application-side ownership alone did not make that safe.

Four registered Vulkan patches now give initialization eight exclusively leased
command pools/buffers/fences, and route submit, present and explicit queue-idle
through one queue host gate. Slots are claimed before waiting/resetting; a
retirement pass cannot touch a leased slot. Resource destruction and GPU fence
waits occur outside queue and slot mutexes. Default-layout creation completes
its private fence before returning, retaining synchronous creation semantics.
DeviceLocal buffer initialization also completes its own copy before returning;
this changes its prior asynchronous behavior and needs startup measurements.
Normal streamed map/ray/item buffers use null initial data; frequent RmlUi
compiled geometry uses Upload memory. No sub-millisecond driver-wait claim is
made: fence waits retain the backend's infinite API timeout, with runtime
watchdogs remaining independent protection.

Texture init-data uploads bypass presentation semaphores and wait their own
fence before retiring command references, so a caller need not submit another
frame merely to release a device. Normal queue submission commits epochs,
queries, retained command buffers and consumed WSI metadata only on successful
Vulkan submission. Completed command buffers and deferred resources are released
outside submission/deletion locks. The host's separate WorldFrame repair moves
independent AS submissions before surface acquisition; a host-call mutex alone
cannot fix early consumption of presentation semaphores.

Pins remain unchanged. The bootstrap verifies the exact ordered registered
patch stack in a temporary Git index, including overlapping patches, and refuses
unregistered tracked edits. CPU evidence: six modified Vulkan translation units
pass syntax checks; the production queue fixture exercises 12 concurrent
recorders against eight slots, 481 submissions and 483 resource releases,
including abandoned recording, rejected submission, delayed fences, reentrant
retirement, and concurrent present/idle calls. Four patch-stack tests cover
fresh application, idempotence, registered-prefix upgrades and refusal/preservation
of unrelated edits. Logs are `logs/build/hq200-vulkan-init-{syntax,concurrency-2,patches,patch-tests}.log`.
These are CPU/syntax checks. Canonical build30 and real 1440-output tiled Vulkan
reproduction, DX12 control and same-binary AS-capacity timing remain separate.

Failure-path follow-up: a private wait returning `VK_ERROR_DEVICE_LOST` leaves
its slot active and retains staging until device-queue destruction. The existing
device destructor releases upload/readback heaps before that final queue destroy.
Build31 addresses this ordering with an appended registered terminal cleanup
patch; successful synchronous creation already retires staging before return.
Only successful fence completion or `VK_ERROR_DEVICE_LOST` releases active
retained staging. Timeout/arbitrary wait errors retain ownership. Completed
fences release staging even when reset fails; a sticky queue error prevents
reuse of their signaled fences. Device loss from recording, submission, wait or
reset poisons subsequent initialization, and dominates earlier reset errors.
This is fail-closed cleanup, not device recovery. An arbitrary final-teardown
wait failure remains outside this focused fix. Actual hardware loss is untested.
Expanded production-queue CPU fixtures, exact patch-stack verification and six
Vulkan syntax checks pass in `logs/build/hq200-build31-terminal-{probe,patches,syntax}.log`.

### Build30 matched AS scheduling and lifetime results

Six alternating same-binary DX12 HQ200 captures passed full 299-tile residency.
Executable/shader/managed payload, driver/host, manifest/content receipts,
settings and light settings identities match. All output is 2560×1440; each run
records 2,335 internal 1280×720 frames, 48 or49 initial 1707×960 frames and eight
at each intermediate step. `logs/client/hq200-build30-as-comparison.json` contains
the per-run data and exact scope.

Capacity1/4 median complete-map request spans are 20,263.345/16,374.881 ms: a
3,888.464 ms (19.19%) reduction. Every pair improves by at least3,699.374 ms;
within-mode three-run ranges are134.069/304.408 ms. This is a residency-overlap
benefit, not a shader/FPS improvement. Upload bytes are identical1,745,204,792;
summed asset-worker CPU medians are4,767.739/4,724.304 ms. Actual in-flight AS
work peaks at one/two; these results do not establish that capacity4 is needed
instead of2. Bounded default4 is supported by timing and short lifecycle tests,
with the scoped fixed-phase native1440 DX12 image comparison below.

**Initial required-visible/collision playable startup remains unmeasured.**
The all-299 duration is separate from that gate and cannot establish a5/8-second
playable-startup failure or pass. Earlier automated per-case
`startup_budget_disposition` values inferred from full-manifest duration are not
valid evidence for this initial-playable gate; preserved raw artifacts retain
that historical label, and the comparison report explicitly supersedes it.

| Median observed peak, bytes | Capacity1 | Capacity4 |
|---|---:|---:|
| Tile reserved GPU bytes |341,167,008|45,491,032|
| Tile resident geometry/AS bytes |804,216,500|804,216,500|
| Shared resident texture bytes |1,319,998,448|1,319,998,448|
| Simultaneous tile resident+reserved+retired+texture |2,319,481,896|2,143,036,200|
| Renderer GPU allocation estimate |2,944,035,443|2,944,035,443|
| Sampled DXGI process-local usage |3,348,824,064|3,214,086,144|
| Sampled process working set |823,705,600|826,093,568|

Each category is its own sampled peak; the simultaneous sum is computed per
owner sample rather than summing category maxima. Reserved GPU backlog falls
with overlapping completion; this does not prove the separate pending-Ray
collision CPU ownership bound. DXGI usage may lag allocation and is distinct
from logical byte estimates. These static tests have zero tile retirement.

All528 after-authority AS submissions per run have schema4 GPU timing coverage
(264 builds and264 compaction/TLAS submissions); total GPU equals main plus
external AS. The70 pre-authority submissions remain outside the rendered-frame
GPU CSV. AS GPU sums are approximately70 ms per run; the .2 ms soft CPU allowance
still overruns, and no hard deadline is claimed. Coarse Windows thread CPU
counters are quantized in15.625 ms steps and do not qualify a3 ms CPU budget.
Settled final300-frame GPU medians are10.08846/10.09810 ms, with no supported FPS
gain or5 ms frame-budget pass.

Separate native1440 generated17-tile routes pass Vulkan and DX12 with26 evictions
and12 cancellations each, clean RHI validation and zero pending shutdown
retirement. Vulkan's late-corrupt176.glb control reaches normal rendering, then
exits1 with the exact `tile_prepare_failed` reason and clean renderer teardown;
no acquired-surface secondary error occurs. Its expected-failure receipt notes
an interleaved metric marker, so it is not clean performance-parser evidence.
Receipt: `logs/client/hq200-build30-queue-lifetime-review.json`. These are short
ownership controls, not long-soak or fixed-phase image-quality acceptance.


### Explicit initial-readiness diagnostic (build31)

`capture_tile_world.py --startup-readiness` explicitly enables the default-off
owner diagnostic. The helper verifies its exact activation marker and records
that diagnostic scans are included in frame CPU cost. Disabled/missing telemetry
remains unknown; requested-but-missing activation fails the harness. These runs
must not be compared as identical-cost timing runs with instrumentation disabled.

Report schema4 separates full-map request duration from the post-present
`world_initial_playable` guard. It validates actual authority/collision,
requested/visible coverage, RT guard, session/generation and exact integer set
identity. The candidate marker never qualifies final readiness. A timely
conservative final guard may prove its **native-main-entry** bound only; this
clock excludes OS process creation and loader time. A late all-manifest RT
superset does not prove that earliest required playability missed 5/8 seconds.
Process-launch readiness remains unknown without a matching process clock.

Eight readiness tests and twelve tile-report tests pass, including slow full-map
completion with unknown initial readiness, missing activation, damaged predicates,
first-session isolation, candidate/final distinction and exact uint64 identities.
Evidence: `logs/build/hq200-build31-{readiness-tests,tile-report-tests}.log`.
Canonical build31 passes; instrumented runtime qualification is separate.


### Build30 fixed-phase AS quality control

Capacity1 and4 both pass the native2560x1440 DX12 RHI control at exact
ready-frame3000 with all299 tiles resident, fixed sampling, identical camera,
lighting and delta. Build/managed/shader/driver identity, every tile/texture input,
settings and lighting JSON match exactly; observation metadata differs only in
readback duration. This is one static view, separate from the timing runs.

The frames are **not pixel-identical**: RGB mean absolute difference is
0.00192175 codes, RMSE0.0673354, maximum14, p99=0 and p99.9=1. Exactly9,419 pixels
(0.255507%) differ; zero exceed16 codes, and alpha matches. Both originals, the
16x difference and a native-resolution foreground crop were inspected. The
strongest difference cluster is the foreground bronze bollard/grating, with
scattered distant facade/metal-edge pixels. No gross new geometry holes or shadow
leaks were observed. The exact source of residual differences is unproven; these
metrics are evidence, not a universal perceptual threshold.

This scoped control supports retaining bounded default capacity4 alongside the
19.19% matched full-residency improvement and both-backend lifetime controls.
Actual timing in-flight work peaked at2; there is no capacity4-versus2 evidence.
Motion, all materials/views, Vulkan fixed-phase parity and initial-playability
budgets remain separate. Evidence and image hashes:
`logs/client/hq200-build30-as-quality/comparison.json`; original cases
`inflight-1/tiles-dx12-uwoljc8j` and `inflight-4/tiles-dx12-6rp90se_` beneath that
folder. The difference visualization and three-column foreground crop remain
beside the report.
