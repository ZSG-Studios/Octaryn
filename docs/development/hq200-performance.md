# HQ200 implementation and qualification

The accepted target is reconstructed 2560x1440 on RX 9070 XT / Ryzen 7800X3D,
with real engine frames, RT shadows/reflections, geometry and gameplay retained.
Internal resolution is 1280x720 through 1920x1080, initially 1707x960, using
pinned FSR 2.2.1 and full-resolution UI. This contract is **not met**.

User direction on 2026-09-27: all subsequent rendering tests use 2560x1440
output. Native controls also render internally at 2560x1440; HQ200 retains
the agreed reconstructed internal-resolution bounds. Earlier smaller captures
remain diagnostic evidence and do not satisfy the new resolution requirement.

## Enforced allocations

`Performance/PerformanceProfile.h` owns the client profile. Select explicitly
with `OCTARYN_CLIENT_PERFORMANCE_PROFILE=HQ200`. It requires RT/PBR, Ultra ray
quality, 1024 m trace settings, uncapped presentation and a 4.4 ms GPU feedback
target. The controller preserves its resolution floor and records overruns.
`tools/validation/hq200_budget.py` checks joined complete frame records; it
rejects missing frames, old timing scopes, captures and disabled requirements.
Its render-budget result does not assert whole-contract acceptance.

| GPU feature | Allocation ms |
|---|---:|
| Visibility | 0.35 |
| Opaque geometry/materials | 0.65 |
| Direct/ambient/AO | 0.30 |
| Shadows | 0.45 |
| Reflections | 0.90 |
| Transparency | 0.35 |
| Atmosphere | 0.25 |
| Dynamic geometry/motion/effects | 0.25 |
| Reconstruction/post | 0.60 |
| UI | 0.10 |
| Streaming/AS | 0.20 |
| Reserve | 0.60 |

Whole-frame mean must be <=4.5 ms, p99 <=5 ms and worst <=8.33 ms.
Active client frame-thread p99 must be <=3 ms. Complete authoritative tick p99
must be <=6 ms at 60 Hz. These overlapping timelines are never added together.
Normal items are 1,000 total / up to 256 continuously awake; 10,000 awake is a
separate 14 ms p99 stress case, with every missed 16.67 ms cadence reported.

## Implemented checkpoint

- GPU timing includes uploads recorded in the main frame. Build28 auditing
  found separately submitted map BLAS/compaction work was outside that interval;
  loading GPU totals are incomplete until external submission timing is wired.
  Static settled runs without those submissions retain their scope. Schema4 lighting timestamps
  separate classification, intersection, hit shading, recovery and experimental
  screen coverage/tracing. Clouds, glass and reactive copies have exclusive
  spans; older mixed forward spans cannot qualify feature budgets. Dynamic
  geometry/motion are exclusive spans. Per-frame records include active/output
  dimensions, required path activation, item counts and memory estimates.
- Bounded asynchronous client profile writers fail evidence on saturation or
  write failure rather than silently dropping rows or blocking the frame thread.
- Optional compact map reflection queues preserve alpha intersections and
  secondary visibility. Build21 adds current-frame recovery statistics and a
  bounded priority refinement queue. It remains opt-in with
  `OCTARYN_CLIENT_RT_QUEUED=1`; neither quality nor speed is approved.
- Reflection targets/queues retain maximum DRS capacity; changing active extent
  invalidates incompatible history without reallocating those resources.
- Original module-owned coin/apple/torch/pebble GLBs are instanced from typed
  authoritative poses, with per-object motion and shared immutable BLAS meshes.
  Client pose interpolation and fence-safe item retirement are included in
  build21. No item ID-to-mesh rules are hardcoded into the renderer.
- [Streaming implementation](hq200-streaming.md): background resource allocation,
  resumable material work, 2 MiB pumps/256 KiB slices, fence-safe cancellation,
  bounded preparation and 16,384-triangle cooking. Packaged HQ200 selects the new
  tiled manifest; explicit monolithic manifests retain the reference workload.
- [Host implementation](host-performance.md): ordered asynchronous saves/logs,
  complete loop/module timing, typed reliable item baselines and bounded deltas.

## Measured decisions

Build18 fresh DX12 720p-internal motion baseline: three 480-frame runs, median
GPU mean 7.28874 ms. That is already over budget at the resolution floor.

Build20 used three alternating matched runs per path with the same binary,
assets, settings and camera workload: 1280x720 -> 2560x1440, FSR custom scale
0.5, Ultra shadows/reflections, direct drawing, uncapped, zero captures.

| Build20 path | Median GPU mean | Median GPU p99 |
|---|---:|---:|
| Existing adaptive | 7.72496 ms | 10.03255 ms |
| Queued, full-direction recovery | 10.82080 ms | 13.30108 ms |

The queue's first recovery implementation is rejected as a performance
candidate. Recovery alone averaged approximately 4.42 ms; intersection 1.47 ms
and hit shading 1.11 ms. Full evidence is
`logs/client/hq200-build20-comparison.json`, with each run's complete distributions.
These camera-only runs do not establish actual traversal or populated gameplay.
Build18 and build20 have different timing scopes and are not a matched speedup.

Build19/20/21 `octaryn_all` builds pass. Build20 queued DX12 and Vulkan 640x360
smokes pass graphics validation and EXIT=0; inspected settled views closely
match the reference. These short smokes are not platform performance approval.
Build21's first DX12 queued 640x360 validation smoke also passes.

Build22 `octaryn_all` passes. Its first fine-grained queued 720p run averages
11.20849 ms GPU, p99 15.24569 ms. Recovery refinement averages 2.71473 ms,
base recovery 1.81967 ms, intersection 1.69159 ms and hit shading 1.08947 ms.
This single diagnostic comparison is not a qualified speedup. Evidence:
`logs/client/hq200-build22-queued720/map-dx12-on-0-9z3sgfjx`.

Removing the single authority job's worker submission/wait improves the normal
fixture's three p99 results to 2.3985 / 15.0858 / 4.3190 ms. The 10,000-awake
stress results are 12.815 / 8.925 / 31.338 ms. Both groups still fail one run;
neither target is qualified. The fixture has no contact stress or graphics.
All before/after evidence, including tails, is retained in
`logs/server/hq200-authority-inline-comparison.json`.

The eight-voice OpenAL loopback workload now runs in the real client, after
correcting the bundled sound-definition path. Loopback mixes real audio without
playing benchmark sounds on the desktop. This validates workload activation,
not physical-device latency or an isolated audio performance budget.

Real four-item client captures prove typed pose consumption, mesh/material
rendering, dynamic TLAS participation and RT shadows. They exposed stale moving
item silhouettes in reflection history. Native temporal-off comparison removes
those trails, identifying secondary reflected-object history as a remaining
quality failure. The initial FSR capture's harness rejected equivalent +pi/-pi
spawn yaw; subsequent checks use angular equivalence. Preserve the original
failed evidence rather than retroactively claiming it passed.
Build22's dynamic-hit history tags still leave visible trails in consecutive
native DX12 captures. That fix is not visually qualified. The evidence is
`logs/client/hq200-build22-items-native/map-dx12-on-0-a4tygg7e`.

Build23's three alternating DX12 comparisons use 2560x1440 output, 1280x720
internal resolution, FSR and Ultra RT. Median GPU means are 7.17022 ms for the
existing path and 8.65746 ms for coherent queued recovery; median per-run GPU
p99 values are 8.81016 and 9.92438 ms. Coherent recovery is approximately 21%
slower and remains disabled. Neither meets HQ200. These are camera-only tests,
not populated gameplay. Full distributions: `logs/client/hq200-build23-comparison.json`.

Build23's six authority fixture runs pass their execution budgets: normal
1,000-item/256-awake p99 is 0.1861 / 0.1902 / 0.1925 ms; 10,000-awake stress
p99 is 3.9516 / 3.9664 / 3.9418 ms. These sum all owner work between consecutive
authority completions, including intervening zero-tick transport pumps. There are no measured advancing ticks over
16.67 ms, no service gaps over 33.33 ms, and measured GC pause deltas are zero.
Owner-work spans exclude worker CPU, pacing sleep and profiling serialization
after the completion timestamp; service gaps include intervening sleep and instrumentation.
This qualifies the airborne/sleeping fixture with an idle player only. It does
not establish contact stress, movement, graphical gameplay or freshness of all
10,000 replicated poses. Evidence and cycle attribution:
`logs/server/hq200-build23-authority-comparison.json`.

The build23 1440p-output graphical fixture renders all 1,000 authoritative items
with 256 awake, eight audio voices, RT and FSR. Prewarmed TLAS and snapshot
allocation counts remain constant through 357 rebuilds. Captures and graphics
validation disqualify this run from timing acceptance; moving-reflection quality
remains open. Evidence: `logs/client/hq200-build23-items1000/map-dx12-on-0-j8bh8zz8`.

Build24 `octaryn_all` passes. Five native 2560x1440 moving-item diagnostics
complete with graphics validation: DX12 adaptive, temporal-off reference,
full-fresh temporal reference, full-fresh with spatial filtering, and Vulkan
full-fresh with spatial filtering. Reviewed adaptive frames retain bright moving
item trails; reviewed full-fresh frames remove those trails even with filtering.
This points to fresh-direction coverage/secondary-motion validity, not filtering
alone. These diagnostic controls are not a cost-qualified fix or full visual
acceptance. Evidence: `logs/client/hq200-build24-reflection-diagnostics.json`.

The build24 1440p actual-input floor route completes with eight audio voices.
Its separate uncaptured run joins every route frame to GPU/CPU records, with
mean 3.104 ms, p99 3.890 ms and worst 6.376 ms. This simple fixture is not Bistro,
contains no item workload and is only 13 seconds. The historical forward GPU
span mixes clouds, reactive copy and glass, so its per-feature attribution is
not qualified. Evidence: `logs/client/hq200-build24-route-timing1440/map-dx12-on-0-p0csboaw`.

The first build24 full 299-tile Bistro startup at 1440p fails during streaming
at renderer frame 376. Its failure-stage label is stale (`atlas_encoder`): CPU
tracing already reached `temporal_begin`, just before tile admission. Preserve
this original failure while investigating; the later acquired-surface error
occurs during shutdown. Evidence: `logs/client/hq200-build24-tile-qualification/dx12/startup/tiles-dx12-3mzeyxft`.

Build25 fixes the reproduced tiled collision failure: tiles 176 and 226 each
contain 16,384 rendered triangles but none above Box3D's unchanged collision
area threshold. Valid empty collision is now distinct from malformed input or
native build failure. All 299 tiles pass CPU preparation/publication tests.
The 600-frame DX12 retry exits cleanly but remains incomplete at 111/299 tiles;
the separate 2,400-frame 1440p retry reaches all 299 tiles with graphics
validation and a reviewed capture. Final tile readiness is 22.562 seconds from
request: loading budgets still fail. See `hq200-streaming.md` for exact scope.

The experimental screen-space candidate exposed a DX12 persistent-pipeline
cache-key collision: identical optimized shader bytecode could have different
root signatures. Build26 includes a registered standalone-RHI patch hashing
serialized root signatures into versioned graphics/compute PSO keys. Unknown
external compute signatures bypass persistent caching. Existing caches are
preserved. Build27 `octaryn_all` and first/warm DX12 1440p runtime captures pass
graphics validation, with 81 pipeline cache hits and no misses in each run.
The second buffered frame now receives the experimental sixth G-buffer
attachment, fixing a separate null-resource crash exposed after the cache fix.

Screen-space intersections remain disabled by default and unqualified: the
sampled depth plane alone cannot prove absence of opaque subpixel gaps or
blockers. Optional LOD surfaces are now excluded. Exact triangle provenance and
conservative opaque coverage are required before claiming preserved occlusion.
Build27's two instrumented runs resolve only 0.3492% / 0.3459% of attempted
screen rays after the first 120 records. The additional screen traversal is
not justified by this yield. These runs include validation/captures/atomic
counters and are not timing acceptance. Evidence:
`logs/client/hq200-build27-screen-review.json`.

A private malformed tile injected after startup now produces the original
`tile_prepare_failed` at renderer frame 364 with stage `tile_pump`, exits
nonzero and drains resource retirement to zero. Shutdown no longer reacquires
the already acquired swapchain image and masks the original failure. The
packaged source tile is unchanged. Evidence:
`logs/client/hq200-build27-forced-tile-failure-review.json`.

Build27 also captures matched lossless/BC7 tiled Bistro views at native 1440p,
ready frame 3,000, with all 299 tiles resident. Bundle/shader hashes, driver,
settings, camera, current sampling phase and actual lighting match. Reviewed
images show no obvious new difference in this view; mean absolute RGB channel
error is 0.1893/255 and RMS 0.5070/255. The codec package reduces texture payload
from 1,319,998,448 to 841,716,572 bytes (36.23%); masked/color-alpha and data
textures retain lossless storage. This is one static view, with potentially
different earlier temporal residency history, not motion/material acceptance or
loading performance. Defaults remain lossless. Evidence:
`logs/client/hq200-build27-tiled-codecs/quality/comparison/comparison.json`.

## Open gates

Build28 `octaryn_all` passes. The optional map-only temporal shader preserves
all 360 fixed-phase ray-counter records and produces pixel-identical native
1440p images in 12 consecutive frames around a camera cut. Three alternating
DX12 timing pairs at 1280x720 internal / 2560x1440 output reduce median GPU mean
from 7.4594 to 7.0298 ms (5.76%). Reflection tracing falls from 3.8134 to
3.3649 ms; whole-frame median p99 remains 8.8376 ms, above contract. This is
camera-only motion with no populated gameplay. The separate Vulkan triplet
reduces GPU mean from 6.7757 to 6.0720 ms (10.39%), with whole-frame median
p99 7.6574 ms. Its 12 native cut images and all 360 ray-counter records also
match exactly. Build29 selects this specialization by default on Windows DX12
and Vulkan, retaining the actual-scene guard and explicit generic control.
Broader gameplay quality remains unqualified. Evidence:
`logs/client/hq200-build28-map-only-comparison.json` and
`logs/client/hq200-build28-map-only/quality/comparison/comparison.json` and
`logs/client/hq200-build28-map-only-vulkan-comparison.json`.

Build28 loading triplets reduce summed lossless asset-worker CPU time from
15,125 to 4,506 ms by reusing validated resident textures before DDS decoding.
These overlapping worker totals are not elapsed loading times. Median request
span to all 299 tiles changes from 21,388 to 20,415 ms; required playable
readiness is not established. BC7 with reuse reaches full residency in 19,203 ms
in the same scoped test. Evidence: `logs/client/hq200-build28-loading-comparison.json`.

Build29 builds successfully and its DX12 tiled graphics-validation capture
passes. GPU schema 4 matches all 528 post-startup independent AS submissions
to measured spans, charging them to total and streaming work. Another 70 AS
submissions belong to startup. Its Vulkan tiled run fails with concurrent
queue/command-buffer use during implicit resource initialization. Investigation
also found external AS submission consuming synchronization intended for the
acquired presentation image. This streaming candidate is not qualified until
both ownership defects are repaired and independently retested. Evidence:
`logs/tools/build29-external-as-dx12.json` and
`logs/client/hq200-build29-as-smoke-vulkan/tiles-vulkan-yh4o797p/client.log`.

The build29 opt-in cloud occlusion predicate skips marching only when opaque
depth is strictly in front of the cloud slab entry. Three DX12 pairs reduce
cloud mean from 0.6693 to 0.1199 ms and GPU mean from 7.0904 to 6.4740 ms.
All 12 native 1440p cut captures and 360 ray-counter records match exactly.
Frame p99 remains 8.6777 ms; sky plus clouds still totals about 0.304 ms.
This scoped camera-only improvement is not a passing gameplay budget or
Vulkan qualification. The option remains disabled by default. Evidence:
`logs/client/hq200-build29-cloud-dx12-comparison.json`.

Build30 rebuilds the patched standalone RHI and passes `octaryn_all`. Separate
DX12 and Vulkan graphics-validation runs publish all 299 tiles and retire
cleanly. Both generated camera routes exercise 26 evictions and 12 cancellations;
a malformed late Vulkan tile preserves the original error and retires to zero.
These are bounded ownership checks, not the long gameplay acceptance suite.
Evidence: `logs/client/hq200-build30-queue-lifetime-review.json` and
`logs/tools/build30-external-as-{dx12,vulkan}.json`.

Three same-binary DX12 pairs reduce median request span to full 299-tile
residency from 20.2633 s with one AS lifecycle to 16.3749 s with capacity four
(19.19%). Actual overlap peaks at two. All pairs improve by at least 3.699 s,
exceeding the largest within-mode range of 0.304 s. Content, identities and
1,745,204,792 upload bytes match. Settled GPU time remains about 10.1 ms; this is
a loading improvement, not an FPS gain. Initial playable readiness is still
unmeasured in these runs. The matched native1440 fixed-phase static images have
RGB mean absolute error 0.00192/255, maximum difference 14, and 0.256% changed
pixels. Reviewed differences are sparse, strongest on bronze/grating details;
their cause remains unproven. This is not bit-identical or motion qualification.
Evidence: `logs/client/hq200-build30-as-comparison.json`.

GPU schema 4 covers the main frame and explicit AS submissions. Private backend
initialization outside those spans remains unmeasured, including in DRS
feedback. Complete streaming GPU budgets cannot be qualified from this sum.
Likewise, the CPU thread-time samples in these runs are quantized at 15.625 ms
and cannot establish the 3 ms active CPU budget.

Build30 completes 15 matched camera-only timing runs, all at 2560x1440 output
with 1280x720 internal rendering. Map-only shadows reduce DX12 median trace
mean from 1.4432 to 1.2518 ms and GPU mean from 6.4347 to 6.2705 ms. On Vulkan,
cloud occlusion reduces GPU mean from 6.1331 to 5.4034 ms; map-only shadows
then reduce it to 5.3297 ms. The shadow GPU improvement exceeds the observed
within-variant range on both backends, but Vulkan's wall-frame improvement
alone is within run noise. Every run exceeds the numerical frame mean and p99
limits. The full HQ200 checker rejects these fixed-resolution custom profiles
as ineligible; its nonzero exit is not a completed contract check.
The combined candidate's median whole-frame p99 is 8.1994 ms on DX12 and
7.0168 ms on Vulkan, with worst individual frames of 11.1016 and 44.2276 ms
respectively; those outliers remain in the evidence.

Native 1440p cut sequences are pixel-identical, with all ray-counter records
unchanged. A separate distant-occluder scene visibly preserves the three
shadow-fade bands on both backends. These scoped checks support default
promotion on qualified Windows DX12/Vulkan in the next build; they do not
resolve existing reflected moving-object trails or establish populated
gameplay quality. Evidence: `logs/client/hq200-build30-render-comparison.json`
and `logs/client/hq200-build30-render-quality-review.json`.

Build31 passes the patched RHI build and `octaryn_all`, enabling the scoped
Windows DX12/Vulkan cloud and shadow defaults. Its Vulkan all-299-tile RHI
capture passes and emits the new opt-in readiness event at 28,966.132 ms from
main entry. This deliberately conservative all-manifest RT guard does not
establish earliest required playable startup; process-launch latency and the
5/8-second acceptance remain unqualified. Disabled readiness diagnostics do
not scan the manifest. Evidence and scope:
`docs/development/build31-readiness-validation.md`.

An opt-in fused reflection variant evaluates secondary visibility before
unneeded base-color/metallic-roughness textures, preserving normal maps and
emissive evaluation. Both backends reproduce all 12 native1440 cut images
exactly, and all 360 rows of existing ray counters match. About 85.4% of hits
skip the unneeded material work in this camera workload. Three quiet matched
pairs reduce DX12 GPU mean from 6.3620 to 6.2432 ms and Vulkan from 5.3365 to
5.0781 ms. Reflection tracing changes from 3.4460 to 3.3338 ms and from 2.7959
to 2.5411 ms respectively. Both GPU gains exceed observed run variation;
Vulkan's wall-frame mean gain alone is within variation. Candidate median
whole-frame p99 remains 8.1142 ms on DX12 and 6.6814 ms on Vulkan; the Vulkan
outlier run has 13.3291 ms p99 and 45.1532 ms worst, retained in the report.
The controlled native1440 material fixture also passes on both backends:
all three images match exactly, including a shadowed emissive reflection and
a sunlit reflection using the secondary surface's normal map. Turning off
emission changes the reflected patch from RGB (252,160,18) to black; replacing
the authored normal with a flat normal removes the lit reference patch.
Build32 additionally passes sleeping-item controls on both backends: all three
native1440 images and360 original ray-counter rows match, actual authority
poses/identities match, and each run records about3.67million dynamic reflection
hits. All eight sleeping/moving fixtures preserve items and stop gracefully.
Moving poses are not synchronized; review finds no new gross material loss but
the existing pale trails and duplicate rough lobes remain plainly visible.
The next build promotes deferred material evaluation only on qualified Windows
DX12/Vulkan direct-map temporal paths. Explicit eager mode remains available.
No temporal repair is claimed. Controlled evidence is recorded in
`logs/client/hq200-build31-controlled-material-review.json`.
Item evidence: `logs/client/hq200-build32-material-item-review.json`.
Evidence: `logs/client/hq200-build31-material-comparison.json` and
`logs/client/hq200-build31-material-quality-review.json`.

Build32 (`octaryn_all` passed) separates compiled ray-counter resources from
diagnostic collection. Three matched quiet pairs at1440 output/720 internal
reduce DX12 GPU mean from6.4008 to6.0468ms (5.53%); reflection trace changes
from3.4537 to3.0950ms (10.39%). Every pair improves and the GPU gain exceeds
the largest within-variant range0.0507ms. Vulkan changes5.3283 to5.3154ms,
within its0.0305ms observed range, so no Vulkan speedup is established. The
next default selection retains compiled-out counters only for qualified
Windows DX12; other backends retain the previous quiet variant.

All four native1440 quality runs exit0 with matched identities and no runtime
validation failure. Images are not exact: DX12 has86 changed pixel observations
across12frames, Vulkan26, out of44,236,800 each; maximum channel errors9/255 and
10/255. Full images and enlarged edge crops show isolated differences with no
obvious structural change; their cause is unproven. The frame mean/p99 numerical
contract gates still fail. These custom fixed-resolution comparisons are not
eligible for the complete HQ200 gameplay contract. Reports:
`logs/client/hq200-build32-counter-comparison.json` and
`logs/client/hq200-build32-counter-quality-review.json`.

Build33 (`octaryn_all` passed) confirms the combined production selections.
With counters compiled out on DX12, deferred materials reduce GPU mean from
6.0590 to5.9920ms (1.11%); with the retained quiet counter variant on Vulkan,
5.3261 to5.1160ms (3.94%). Both gains exceed observed ranges and every matched
pair improves. Native1440 eager/deferred images are exact on both backends.
Do not add the earlier independent gains: shader optimizations interact.

Forced DX12 Wave32 improves total GPU only0.0150ms, within0.0207ms variation;
Wave64 regresses total GPU22.55% and reflection tracing45.90%. Driver selection
remains the default. Diagnostic shaders observe32 lanes on DX12 and64 on Vulkan,
but these do not establish the quiet driver's chosen widths or explain its
code-generation difference. Wave64 differs in900 pixel observations across12
native frames (maximum6/255) and tiny ray counts; no exact-equivalence claim.

Current deferred-driver wall-frame means (median of three runs) are6.1712ms
DX12 and5.6335ms Vulkan; corresponding p99 values7.7683 and7.7392ms. Worst
individual frames are8.2925 and63.6354ms; Vulkan's48.3267ms and63.6354ms outliers
remain in the results. GPU, mean andp99 gates still fail. These short camera-only
fixed-resolution comparisons do not qualify the complete gameplay contract.
Evidence: `logs/client/hq200-build33-material-wave-comparison.json` and
`logs/client/hq200-build33-material-wave-quality-review.json`.

Build34 (`octaryn_all` passed) rejects the static opaque visibility-witness
candidate. All 12 native 1440p camera-cut images match exactly on each backend;
360 rows preserve the previous counters after balancing saved versus actual
secondary queries. The diagnostic saves 26.92% of secondary queries on DX12
and 26.00% on Vulkan. Nevertheless, three matched quiet pairs at 1440p output /
720p internal increase total GPU time from 5.9983 to 6.7481 ms on DX12 (+12.50%)
and 5.0717 to 5.9690 ms on Vulkan (+17.69%). Reflection tracing increases from
3.0512 to 3.8335 ms and 2.5383 to 3.4273 ms respectively. Every pair regresses.
The witness is rejected; its reduced query count is not an optimization
result. Controlled alpha/edge and populated fallback qualification is not run
after this performance rejection. Reports:
`logs/client/hq200-build34-witness-comparison.json` and
`logs/client/hq200-build34-quality-review.json`.

Build35 (`octaryn_all` passed) removes the failed witness implementation and
adds opt-in complete renderer/fence records. Native 1440p quality remains exact
against build34's original path on both backends: 12 cut frames and all 360
diagnostic rows match. The remaining ray-counter and world-summary CSV writers
now use bounded asynchronous output with explicit capture failure on data loss.

Both 1,600-frame retirement captures pass exact camera/CPU/GPU/fence joins.
The previous 48–64 ms stalls do not reproduce. Keeping all frames, wall p99 /
worst are 12.3863 / 16.9298 ms on DX12 and 11.7207 / 14.0969 ms on Vulkan.
The separately declared ready-frame >=120 window retains 1,480 frames, with
p99 / worst 7.8151 / 8.5473 ms and 6.6811 / 7.2676 ms respectively. Its worst
frames contain actual fence calls of 7.7463 ms and 5.8543 ms, each retiring an
identified older submission. These instrumented runs are diagnostic, not FPS
qualification. They do not identify driver/OS causes or prove a stall repair.
The first GPUPerfAPI hardware attempt fails its SDK version-order check before
profiling starts; its failed evidence is retained. Reports:
`logs/client/hq200-build35-retirement-quality-review.json` and
`logs/client/hq200-build35-retirement-analysis.json`.

Build36 (`octaryn_all` passed) corrects the GPUPerfAPI release/API version
mapping and completes eight single-pass DX12 hardware captures at 1440p output
/ 720p internal. Three requested counter combinations are rejected because
they require multiple passes. The reflection sample reports 99.72% compute
busy in one capture; separate captures report 88.99% L2 hit rate, 97.57% memory
unit busy and 0.73% request-interface backpressure. Ray-box and triangle-test
counts are 140.67 million and 105.18 million in different frames. These are
sample-scoped diagnostics with different sampling phases: do not combine them
as one frame, infer occupancy or divide by whole-frame logical ray counts.
Neither these captures nor the version fix establish a rendering speedup.
See `gpu-hardware-counters.md` and
`logs/client/hq200-build36/gpa-traversal-memory-audit.json`.

Build37 (`octaryn_all` passed) rejects the first current-frame rough-quad
reconstruction prototype after a matched DX12 pilot. Native 1440p diagnostic
captures exercise reconstruction for only 3.96% of receivers; full-current
fallback increases logical reflection queries approximately sevenfold. At
1440p output / 720p internal, quiet total GPU mean rises from 5.9757 to
17.4757 ms. Combined reflection tracing and reconstruction rises from 3.1478
to 14.3951 ms; the resolve alone costs 13.8961 ms. The single pair establishes
a severe rejection, not a new qualified baseline or a three-run speedup.
Actual consecutive native images were reviewed, but quality acceptance is
not claimed. The prototype was removed after archiving its exact source
and evidence. No populated acceleration was implemented by this candidate.
Report: `logs/client/hq200-build37-rough-quad-pilot-review.json`.

Build38 (`octaryn_all` passed) restores the retained renderer and tests the
existing original-order 299-tile layout against monolithic Bistro. Every
4,146,017 triangle preserves its material, position, UVs, color and winding;
the extra tile reload renormalizes normal/tangent components by at most
1.79e-7. Actual cooked DDS payload sets are identical. Three matched native
1440p static images have mean RGB difference about 0.0032/255, maximum 11/255;
review found no visible geometry loss in this view, without claiming bit parity.

The declared fully resident 600-frame window at 1440p output / 720p internal
rejects this layout as a speed promotion: total GPU mean rises from 6.2999 to
8.1142 ms, reflection tracing from 3.2613 to 4.6976 ms and shadow tracing from
1.3735 to 1.7098 ms. This one paired static pilot is not a noise-qualified gain,
motion benchmark, loading qualification or proof about every tiled layout.
Keep the existing monolithic rendering reference; the tiled streaming system
remains available for its separate large-world purpose. Reports are under
`logs/client/hq200-build38/geometry/analysis/`.

Final build38 native 1440p camera-cut restoration passes separately on Windows
DX12 and Vulkan: all 12 captured images and all 360 prior diagnostic rows match
the retained build35 path exactly; the five appended reserved counters are zero.
Actual native crops and difference images were inspected. This restores the
tested baseline; it does not repair moving secondary-geometry history or qualify
200 FPS. Report: `logs/client/hq200-build38-restoration-review.json`.

Bounded recovery needs fresh timing and consecutive cut/highlight review.
Secondary reflected-object validity, hybrid screen-space intersections, ordered
glass receiver batches and shadow classification remain open. Item captures
need the corrected reflection history and 1,000-item graphical workload.

Streaming soft pump timers do not prove GPU or CPU budgets. Required readiness
deadlines, speed-derived prefetch, stable collision, 30-minute actual traversal,
20 A-B-A map switches and <=70% OS GPU-budget residency remain unqualified.
CPU preparation reservations do not prove whole-process RSS limits.

The current frame-driven camera streaming stress is not walking/sprinting.
Full acceptance requires three matched 120-second gameplay runs, every cut and
streaming transition included, separate DX12/Vulkan results, quality sequences,
audio/UI load, latency/loss/reconnect/backpressure, and native 1440p comparisons.
Coarse Windows thread-time samples cannot alone qualify 3 ms CPU p99.

Technique references: [AMD hybrid reflections](https://gpuopen.com/manuals/fidelityfx_sdk/samples/hybrid-reflections/),
[AMD denoiser](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/denoiser/) and
[Epic Lumen budgets](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine).
Their published targets are guidance, not measured performance of this engine.
