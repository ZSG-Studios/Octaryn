# Streaming follow-up audit after build31

This is a read-only source/record audit. No new rendering, cooking or performance
implementation was run. The build30 timing case and build30/31 Vulkan RHI cases
have different modes/diagnostics; their values are individual observations, not
a matched cross-build or backend improvement claim.

## Current CPU and GPU limits

Evidence: `logs/client/hq200-build31-streaming-source-audit.json` retains exact
log lines, owner records and GPU frames for three all299 cases.

| Observed maximum, ms | Build30 DX12 timing | Build30 Vulkan validation | Build31 Vulkan readiness |
|---|---:|---:|---:|
| Upload pump elapsed |0.460|0.690|0.586|
| AS lifecycle owner elapsed |0.966|4.178|3.945|
| One AS work operation elapsed |0.769|4.006|3.771|
| Resource allocation worker elapsed |6.634|75.574|63.445|
| Measured streaming GPU |0.55816|0.73472|0.70164|

The worker allocation values are **not frame-thread pump time**. They include
backend creation and private initialization fence waits; their `cpu_ms` label
means elapsed wall time, not measured thread execution. Build31's largest worker
resource call was11.013ms. The slowest measured frame-owner substage was tile178's
AS Build operation at tile-session frame1306:3.771ms work /3.945ms scheduler.
It includes fence creation, encoder creation, descriptor/input preparation,
recording, encoder finish and queue submit. Existing records do not split these
calls, so attributing the spike to one driver call would be speculation.

Build31 upload's0.586ms outlier moved1,593,344bytes; its maximum recorded RHI
upload call was0.028ms and material time was0. Existing limits remain2MiB per
pump and256KiB between time checks. Whole-mip payload freeing, loop work,
resource-state recording and scheduling interruption are unseparated candidates.
Material records are already resumed one primitive at a time (at most five image
handles). Material time and maximum-upload-call fields accumulate within a
builder, so they cannot be treated as independent per-pump subphase durations.

Full tile-pump CPU tracing was disabled in these three captures. The GPU CSV's
`acquire_cpu_ms` spans temporal setup, tile work and surface acquisition; it does
not identify a pure tile pump. The31 record35.6286ms at renderer frame31 therefore
cannot be called a35ms streaming operation. Per-owner AS/upload budgets are
independent soft checks, and pool/accounting/publication/retirement work lies
outside them. The aggregate0.5ms frame-thread streaming allowance is unqualified.

The31 GPU record has254/2400 streaming samples above0.2ms;251 samples have AS alone
above0.2ms. Its worst AS is0.69712ms at renderer frame1227. These full-residency
startup samples are distinct from settled gameplay. Private Vulkan worker
initial-layout submissions are outside both deferred main-frame timestamps and
the external-AS scope. Their GPU execution is not measured by CPU fence-wait
duration. There is no existing private-init execution statistics API. A bounded
diagnostic could reserve two timestamp indices for each of the existing eight
initialization slots (16 total), resolve only after that slot's successful fence,
and publish bounded aggregate counters. This needs an explicit backend extension
and valid timestamp support; it must not allocate per-resource query pools or
share unsynchronized frame query slots.

## Next bounded CPU unit

First add complete owner-pump and narrow AS record/finish/submit attribution,
including publication and resource destruction. Reuse a fixed four-lifecycle
fence/recording capacity instead of creating a fence for each submission. A ready
upload can hand immutable geometry to one isolated recording job only after its
upload fence; the frame owner admits/submits one finished AS packet, retaining
current reservations, cancellation and fence ownership. Backend encoder and
bindless allocator thread safety must be proved before moving recording. Keep
queue submit on its existing owner and preserve one-operation admission. This
moves a concrete whole operation; it does not promise a timer can preempt a driver.

Completed mip/CPU geometry frees and retired AS/resource destruction can then
use bounded retirement batches with retained byte accounting. Do not release
resources before their consumers/fences or silently omit retained payload from
512MiB preparation accounting. Pending-Ray collision ownership still needs a
complete bound. No new parallel preparation slots are proposed here.

A separate source finding is redundant per-map TLAS work: `MapRayResources.cpp`
allocates a one-instance TLAS and `MapRendererRay.cpp` builds/rebuilds it, while
`WorldRaySnapshot.cpp` consumes each map's BLAS directly into the world TLAS.
Removing unused local TLAS on an explicitly world-owned map path could reduce
allocation/submission work, but needs all call-site and readiness-contract review;
this audit does not remove or weaken the standalone path.

## Spatial BLAS cook proposal

`tools/Source/MapTileCook/Partition.cpp` first bins whole triangle centroids into
32m XYZ cells. Within each cell, triangles retain source primitive/material/index
order; consecutive slices stop at16,384triangles or96MiB unique texture payload.
There is no Morton or spatial split inside a dense cell. `Write.cpp` preserves
whole triangles and material assignment. Triangles crossing cell boundaries are
not clipped, so a tile's actual bounds can exceed32m.

Manifest-only evidence is in
`logs/client/hq200-build31-streaming-aabb-audit.json`:

- 299 tile AABBs have7,345 positive-volume overlapping pairs out of44,551.
- Overlap neighbors per tile: median42, p95=131, maximum180.
- 2,294 pairs overlap at least80% of the smaller box volume.
- Median XYZ extents are15.54 /7.57 /16.05m; p95 extents36.69 /32.12 /39.10m.
- Largest extents are44.07 /37.85 /49.30m. Summed AABB surface area is6.33times
  the world AABB surface area.

These are overlap proxies, not measured BVH node visits or traversal cost. They
support a spatial partition experiment; they cannot explain a6.3ms monolithic
versus10.1ms tiled result captured with different cameras/settings.

The proposed offline slice is deterministic spatial ordering/splitting inside
each centroid cell, starting with Morton order and evaluating binned centroid
SAH for dense partitions. Keep all triangles, vertex attributes, UVs, winding,
alpha flags, material transforms and sampler settings. Keep16k/96MiB limits and
actual-vertex conservative bounds; never shrink bounds to centroids. Reorder only
membership; the writer still groups triangles by original material per tile.
Texture-budget splits can increase tile count, so cap/count/memory and collision
mapping must be checked before packaging. Do not increase runtime budgets to make
the candidate fit.

Acceptance requires full canonical triangle/material/attribute conservation,
collision map readiness, strict cooked identities and actual all-resident captures.
Use the same executable, camera/lighting, output1440 and internal resolution,
fixed sampling for quality, then three alternating uncapped repetitions comparing
current tiles versus spatial tiles. A separate same-camera monolithic control
isolates tiled traversal/submission overhead. Compare shadows/reflections GPU,
actual rays, opaque/forward submission, AS bytes/build latency, full-residency,
cancellation and requested-region readiness. Retain the current cook/reference
until gain exceeds three-run variation and images/motion preserve quality.


## Optional Morton candidate source checkpoint

The cooker now accepts a trailing `--order morton`; omitted or `--order input`
retains the original order. Morton uses21bits per centroid axis within each32m
cell, with source primitive/index tie breaks. Packing still enforces the same
16,384triangle/96MiB limits. `cook.json` adds partition order/version/limits;
the runtime manifest schema and default tile ordering remain unchanged. Candidate
mode refuses a nonempty output directory before source/cache reads. Nothing
selects or packages the candidate automatically.

`octaryn_map_tile_cook --compare CONTROL CANDIDATE` loads both cooked sets through the
production loader and compares sorted per-triangle SHA256 signatures including
oriented position/normal/UV0/UV1/tangent/color, material factors/alpha behavior,
texture transforms/samplers/UV selection and encoded image content identity.
Local vertex/material/image numbering is excluded. A matching multiset retains
duplicate triangles and winding. `compare.json` in the candidate contains the
multiset digest, separate content identities and actual tile bounds/overlap
statistics. This offline check hashes every triangle; no duration is promised.

The focused native fixture passes32triangles with deterministic byte-identical
repeat cooks, unchanged default-input output, negative cells and centroid ties.
It rejects winding, UV1, normal, alpha, texture transform, color and image-content
mutations. The existing16k/material/image-bound tests also pass. Evidence:
`logs/build/hq200-morton-tile-fixture.log`. The CLI guard rejects the preserved
lossless comparison directory in `logs/build/hq200-morton-overwrite-guard.log`.
These checks do not establish full-Bistro conservation or performance.

After the coordinated GPU window, use the existing cooker executable with:

```text
SOURCE=octaryn-client/Assets/Maps/main.glb
CACHE=build/release-windows/client/map-textures/main.glb.textures
CONTROL=build/release-windows/client/map-variants-hq200/lossless
CANDIDATE=build/release-windows/client/map-variants-hq200/morton-trial
octaryn_map_tile_cook SOURCE CACHE CANDIDATE 0 3 -20 .6 -.25 --order morton
octaryn_map_tile_cook --verify SOURCE CANDIDATE
octaryn_map_tile_cook --compare CONTROL CANDIDATE
```

These are argument templates, not an executable shell script. Use a fresh
candidate directory. Canonical builds may normally recook input-order packaged
HQ assets when the cooker executable changes; the preserved `CONTROL` directory
must remain untouched. The offline conservation comparison below now passes;
runtime measurement of the Morton candidate remains pending.


## Prepared experiment and further traversal work

`logs/tools/morton-candidate-qualification.json` contains21 serialized stages:
five offline identity/cook/conservation stages, four native1440 fixed-phase
quality controls (input/Morton on DX12/Vulkan), and twelve alternating native1440
uncapped timing cases (three pairs per backend). Six additional monolithic
controls are explicitly blocked until the actual settled tile camera and active
settings/shader variants match. The five offline stages have run; GPU commands
remain unexecuted.

Quality uses exact ready-phase3000 at camera0,3,-20 / yaw0.6 / pitch-0.25;
all manifest tiles and zero preparation/upload work must precede capture. Timing
uses the real stationary authoritative camera and compares common renderer
frames3600..3899, excluding the frame3000 readback. The helper currently permits
an injected camera only for fixed-sampling quality, so timing camera CSV equality
is a precondition, not an assumption. The monolithic control's expected eyeY is
1.938856 and must be checked against both tile variants. Failure to finish by the
common gate remains a recorded failure; any longer paired workload is separate.
Both native paths keep RT, full geometry, materials and native2560x1440 internal
and output dimensions. Initial playability is not inferred from full residency.

The next traversal candidate after measuring Morton is **ray-purpose filtering
of blended geometry before candidate traversal**. Source evidence:

- `MapRayResources.cpp::map_ray_inputs` already marks authored Opaque geometry
  opaque. Do not propose this existing fast path as a new optimization.
- A tile BLAS currently combines all material geometry inputs. Every world TLAS
  map/item instance has mask255; each inline query uses mask255.
- `WorldRayQuery.slang` fetches material metadata for nonopaque candidates, then
  discards Blend when `classifyTransmission` is false. Mask requires the existing
  authored UV/vertex alpha/texture transform/sample test and must remain intact.
- Reflections and shadows use the same world TLAS; offscreen and secondary-hit
  visibility must remain complete. Frustum-only geometry removal is invalid.

A future measured design can build separate opaque+mask and blend BLAS subsets
per spatial tile, with distinct instance mask bits. Queries that currently skip
Blend could exclude its instance mask in traversal; transmission classification
would include both categories. Every triangle remains in one subset and the
material-index lookup must stay exact through an explicit geometry-to-material
mapping or proven contiguous ranges. This increases instance/BLAS count and can
worsen bounds, memory and build cost; it is not authorized as an assumed win.
First use bounded diagnostic candidate counts by alpha class in representative
foliage/glass/chrome views to determine whether ignored Blend visits matter.
No added per-ray counters belong in timing qualification.

Separately, moving items rebuild the combined world TLAS whenever item revision
changes. Static map BLAS are retained and not rebuilt, but all static instances
are re-emitted into that TLAS. Update-enabled preallocated snapshots or a separate
dynamic scene are possible construction optimizations; two-scene queries can
increase traversal cost and require nearest-hit/transmission correctness. Neither
has evidence of reflection speedup yet. Removing the unused local one-instance
map TLAS affects loading/build cost, not a proven steady-ray traversal bottleneck.


The prepared plan was corrected to the canonical executable
`build/release-windows/tools/map-tiles/octaryn_map_tile_cook.exe`.
`map_tile_cook.exe` beside it is the focused-fixture build. Read-only validation
checked all27 planned command syntaxes,12 required executable/input paths and
native usage output; the candidate directory was absent before cooking. Receipt:
`logs/tools/morton-plan-path-validation.json`. The queued preflight also verifies
all preserved variant geometry/image hashes against `variants.json`; bulk content
verification is not claimed by these path checks.

## Offline Morton result (canonical32 cooker)

The five serialized offline stages completed in
`logs/tools/morton-offline-a1c2da067d/result.json`. Cooking took7.517s, sampled
peak working set1,750,872,064B/private1,187,803,136B. Source verification took9.269s
and the full between-mode comparison18.043s. These are offline tool costs,
not launch readiness or the runtime512MiB preparation budget.

The first comparison process returned success but emitted malformed JSON: its
bounds array lacked a closing bracket. The original is preserved in
`logs/tools/morton-offline-a1c2da067d/compare-malformed.json` with a separate
`receipt-validation-failure.json`. A narrow serializer fix, canonical cooker
target rebuild, native fixture and explicit JSON/bounds-array regression all
passed. The comparison was rerun without recooking or changing either geometry
set; logs are `logs/build/hq200-morton-receipt-{build,fixture,comparison}.log`.

All4,146,017 oriented triangles retain every compared vertex attribute, material
parameter, texture transform and encoded image content. Both sorted SHA256
multisets equal
`1d5c2e541dff173dc329c2d4bf01efd77ed9b3ab536ffa51a01af7dd4245c67b`.
Preserved lossless input receipts before/after are identical. The candidate is
`build/release-windows/client/map-variants-hq200/morton-trial`; it is not selected
by default. Its corrected `compare.json` records both exact content identities.

Small GLB-header analysis is recorded in
`logs/client/hq200-morton-trial/offline-analysis.json`:

| Offline property | Input order | Morton within cell |
|---|---:|---:|
| Cells / tiles |45 /299|45 /669|
| Nonfinal cell splits caused by96MiB texture limit |25|524|
| Nonfinal16,384-triangle splits |229|100|
| Vertices / triangle |0.976146|1.010918|
| Material primitives |2,328|7,177|
| Direct geometry payload, no LOD/AS |423,981,560B|436,988,856B|
| Unique encoded images / cooked variants |270 /272|270 /272|
| Shared cooked texture payload |1,319,998,448B|1,319,998,448B|
| Per-tile encoded-image references |3,618|18,480|
| Positive-volume AABB pairs |7,345|13,087|
| Pairs overlapping≥80% of smaller volume |2,294|2,265|
| Median / maximum neighbors |42 /180|33 /296|
| Sum AABB volume |2,740,449.536m³|2,043,566.814m³|
| Sum AABB surface area |637,941.644m²|641,198.518m²|

The split classification follows unchanged cooker logic: each cell's nonfinal
batch ends either at16,384 triangles or before the next material would exceed
96MiB of unique cooked textures. Morton interleaves spatially nearby materials,
losing some original material locality. More texture-limited batches produce
370 additional BLAS/local TLAS objects, more duplicated boundary vertices and
material records, and5.1× per-tile image references despite identical shared GPU
texture payload. This can increase preparation and submission overhead.

The direct geometry delta is13,007,296B; indirect mode without LOD adds100B per
primitive (424,214,360B versus437,706,556B total). World instance descriptors add
an estimated23,680B per snapshot at the64B DX12/Vulkan layout. Actual candidate
BLAS/TLAS and scratch bytes remain unknown until backend size queries and
compaction: identical triangle count cannot establish identical acceleration
structure memory. The reference runtime geometry+AS total804,216,500B is only an
anchor, not a candidate estimate. No GPU query was performed during this cook.

These AABB proxies have mixed results: volume sum falls25.43%, while area sum
rises0.51%, absolute overlapping pairs rise and maximum neighbors worsen.
Pair fraction falls16.49%→5.86% largely alongside the larger tile population;
this is not measured BVH traversal or a speedup. Keep the candidate opt-in until
same-camera, same-binary native1440 quality and three matched timing runs per
backend qualify it, including full-residency and memory costs.
