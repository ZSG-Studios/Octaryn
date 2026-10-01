# Map asset preparation and submission

## Cooked assets

Client packaging builds `octaryn_map_textures` and `octaryn_map_geometry` before
staging map assets. Both staging tools reject incomplete, corrupt, or stale
source data. Cooked files live beside the packaged GLB under `Client/Assets/Maps`.

Texture DDS contract version 3 stores dimensions, full semantic mip chains,
color space, compression mode, and source-image opacity inside the checksummed
payload. Cache keys include encoded source bytes, material mip options, and the
algorithm version. Runtime reads the cache before decoding PNG/JPEG data.

The default cooker produces lossless RGBA8 reference assets. Its explicit
`--bc7` option uses pinned bc7enc_rdo at maximum search quality for opaque color maps;
alpha-bearing, normal/data and non-block-aligned images remain lossless. BC7 output needs
separate image and motion qualification before becoming a packaging default.

For an isolated comparison, first rebuild/check the CPU tool with
`python tools/validation/validate_map_texture_cache.py`, then run
`python tools/validation/prepare_map_texture_variants.py --cook --jobs 4`.
Do not encode while collecting matched GPU performance measurements. This
creates `client/map-variants-opaque/lossless/map.json` and `bc7/map.json` under the build
preset, without replacing the normal cache. Select each with the capture tool's
`--manifest` argument. Geometry, camera settings and LOD files are identical.

The cooker `--compare reference-directory candidate-directory report.csv`
decodes BC7 and reports every mip's RGB error in sRGB code values, PSNR, alpha
error and alpha threshold flips. Normal/data textures must remain byte-exact.
These metrics identify assets for inspection; they do not establish perceptual
equivalence or freedom from temporal artifacts. The first unrestricted color
experiment changed alpha by as much as 250/255, so alpha-bearing images now stay
lossless. Current Bistro payloads are 1,319,998,448 bytes lossless and 841,716,572
bytes with 141 opaque BC7 variants, a 36.23% reduction. CPU base-level RGB PSNR
is 50.41 dB weighted over compressed pixels (40.07 dB worst texture), with no
alpha threshold flips. Opaque blocks can decode alpha 254 instead of 255;
alpha-bearing textures remain byte-exact. The initial broader cook, validation
and staging took 67.77 seconds; the alpha-preserving candidate reused its opaque
blocks and took 7.23 seconds. GPU loading, visual and motion results are separate.

Geometry cooks preserve the optimized original vertex buffer. Two index-only
LOD levels use meshoptimizer attribute-aware simplification with locked
primitive borders. Normals, both UV channels, and colors contribute to error.
Masked and blended primitives retain original geometry. The cache authenticates
source identity and validates every range, index, and finite error value.

## Reference and experimental paths

`OCTARYN_CLIENT_MAP_DRAW_MODE=direct` selects the unchanged per-primitive CPU
submission reference. `indirect` selects GPU frustum culling and one indexed
multi-draw command. Its five-word command ABI matches DX12 and Vulkan; material
selection uses local instance ID plus base instance. DX12 requires shader model
6.8, which is already selected by the renderer.

### Two-phase Hi-Z occlusion (opt-in)

`OCTARYN_CLIENT_MAP_OCCLUSION=1` enables Frostbite-style two-phase occlusion
culling (Wihlidal, GPU-Driven Rendering Pipelines) on the indirect path. A
max-depth pyramid (`MapHiz.slang`, R32Float full mip chain over the depth
allocation) is rebuilt after each phase-1 opaque pass. Phase 1 culls against
the previous frame's pyramid and records per-primitive flags
(0=frustum, 1=drawn, 2=occluded); phase 2 retests only the occluded set
against the fresh pyramid and draws the newly visible, so camera motion never
drops geometry. The box test projects all eight eye-relative AABB corners with
the exact `map_vertex` projection, picks the conservative mip from the screen
rect plus 2 px, and compares nearest depth against the sampled farthest depth
(depth clear 1, LessEqual: smaller is nearer). Blended primitives and the
forward pass are unchanged; ray tracing always uses full-detail geometry.

Per-map cull dispatch is replaced by a fused multi-map dispatch
(`MapCullSet`): every resident indirect map owns a contiguous run of 256-entry
slots in global primitive/argument/flag tables (512 slots, ~16 MB), registered
with one `copyBuffer` per newly resident map, so one compute dispatch per phase
covers the whole resident set and per-map `drawIndexedIndirect` reads its slot
range. Maps without a slot (table exhaustion) fall back to the direct
per-primitive loop; phase 2 skips them since phase 1 already drew them.

DX12/RX9070XT evidence under `logs/client/hiz/` (mechanism) and
`logs/client/fused-cull/` (fused dispatch). The two-phase mechanism is verified
end-to-end (a force-cull probe rendered sky-only; phase-2 revive restores
correct images; RHI-validation run passes). At the 299-tile Bistro wall-blocked
terrace view (2560x1440, upscaler 1, indirect, 2400 frames, mean of last 120):

| config | opaque ms | total GPU ms | encode CPU ms |
| --- | --- | --- | --- |
| per-map dispatch, occlusion off | 1.897 | 15.454 | 4.695 |
| per-map dispatch, occlusion on | 1.654 | 15.892 | 8.684 |
| fused dispatch, occlusion off | 1.301 | 15.184 | 2.763 |
| fused dispatch, occlusion on | 0.901 | 14.875 | 4.522 |

Fusion removes 299 per-map compute dispatches and their buffer state
transitions: −1.9 ms encode CPU and −0.6 ms opaque GPU with occlusion off.
Occlusion on the fused path is net GPU-positive (−0.31 ms total, opaque pass
−31%) for +1.76 ms CPU, and stays opt-in because the win is view-dependent
(open views keep little occluded geometry to recover). Image parity: fused vs
direct MAE 0.44 (noise floor), occlusion on vs off MAE 0.80; no dropped
geometry. Monolithic maps wider than one slot own contiguous slot runs (the
2083-primitive Bistro GLB takes 9 slots) and draw through the same fused path.

With indirect submission, `OCTARYN_CLIENT_MAP_LOD_PIXELS=0.5` enables LOD within a
conservative projected error budget. Zero disables simplification. Near-plane
intersections retain original geometry, and all ray-query/collision geometry
remains full detail. Shader compilation is not runtime parity evidence.

## Streaming upload contract

`prepare_map_asset` runs on a worker and performs parsing, exact mesh optimization,
texture cache reads or decoding, mip preparation, and optional LOD cache reads.
Cancellation is checked between bounded stages and image variants. Prepared
payloads are capped at 256 MiB; parser, decoder, optimization, and collision
scratch must be accounted for separately by the streaming owner.

Before streaming begins, `prewarm_map_pipeline_pool` compiles shared pipelines on
the render owner. `begin_map_renderer_build` requires that prewarm and creates no
GPU resources. The pump records geometry/material uploads in bounded chunks and
texture uploads in block-aligned rows. It checks the CPU time budget between
operations; individual driver calls cannot be preempted. Staging-byte telemetry
includes conservative row/offset alignment. GPU allocation telemetry reports
referenced resource payload sizes, not driver residency or heap commitment.

Texture pool entries are weak. Live maps retain shared texture resources, and
unfinished providers cannot be used by another map. Pipeline resources are
shared for the pool lifetime. Per-map texture totals include shared references;
`map_texture_pool_bytes` counts the shared payload once.
`map_texture_pool_stats` separates ready and pending allocations, and
`map_texture_pool_additional_bytes` reserves only texture keys absent from that
pool. Aggregate renderer statistics use `map_unique_texture_bytes` across maps.
The accounting refresh also removes up to 64 expired weak cache keys. Live
shared textures remain owned by resident maps; old key metadata no longer grows
with every distinct texture visited during traversal.

Material samplers share a separate `MapSamplerCache` across the tile session.
Its field-wise key includes all 16 functional RHI descriptor values; debug labels
do not change sampling and extension chains are rejected. Each map strongly owns
the sampler resources referenced by its material buffer through frame retirement.
The shared cache holds weak references, prunes expired configurations on misses,
and limits live configurations to 64, leaving room within the existing 128-slot
bindless sampler allocation. Limits are unchanged. Monolithic maps use the same
cache with their own lifetime. Allocation errors report create versus bindless
descriptor operations and material/texture context. The CPU sampler probe checks
descriptor identity; the full 137-tile GPU run must verify descriptor sharing.

Encoded image ownership is separately bounded before every model-image copy:
86 MiB per image and 512 MiB in aggregate, below the decoder's signed `int`
input-length limit. External images validate file size and offset, then read
directly into the final vector with an exact-length check. Embedded buffer
views, parser arrays and byte views use the same bounds. These limits govern
the model's encoded copies; they do not bound parser-owned buffers, external
geometry buffers, geometry optimization or collision scratch. They are distinct
from the 256 MiB retained prepared-tile payload limit and do not establish a
hard process peak-memory guarantee.

`NeedsSubmission` means uploads have been recorded, not completed. Call
`map_renderer_build_submitted` only after submitting those commands with a fence.
The builder publishes a renderer only after the fence completes; device-removal
sentinels fail publication. Zero-budget pumps only poll readiness. The streaming
owner must separately require BLAS and authoritative collision readiness before
publishing a tile, and must retire drawn maps behind the last referencing fence.

## Focused verification

- `python tools/validation/validate_map_texture_cache.py`
- `python tools/validation/validate_map_texture_stage.py`
- `python tools/validation/validate_map_lods.py`

The LOD fixture exercises primitive-border retention, original-index preservation,
source/integrity rejection, alpha-material exclusion, and CPU preparation cache
parity/cancellation/budget rejection. GPU publication, upload budgets, and visual
quality additionally require the integrated streaming runtime captures.

## Loading evidence

`python tools/validation/loading_report.py path/to/matrix.json --output logs/client/loading/report.json`
reads saved startup logs and build/asset identities only. It emits JSON plus a
Markdown table, with exactly three matched repetitions required for aggregation.
Every matrix case is a fresh process. The first observed repetition is not a
cold-cache measurement; OS file and shader cache state remains unknown.

Map loading records are individual wall-clock stages. Renderer boot markers
are cumulative milestones; map renderer, temporal and collision durations can
be nested in the map worker interval and must not be added as independent costs.
Event-pump maximum gaps are reported separately from worker elapsed time.
Historical authoritative-ready markers have no timestamp, so readiness is
observed but client-ready latency is unavailable. New `sdl_uptime_ms` and
`session_elapsed_ms` fields keep their distinct clock origins; neither is
silently labeled process-launch latency.

The completed `performance-fixed-1440` matrix contains five variants with three
launches each. Median image preparation ranges from 1,894 to 1,933 ms, map renderer
loading from 3,333 to 3,907 ms, and map worker completion from 5,209 to 5,785 ms.
These are observed fresh-process results, not a controlled cold/warm comparison
with the earlier 8.25-second baseline. See `logs/client/performance-loading/report.json`
for every launch, recorded identities, and missing historical receipt coverage.

Matrix comparisons now check aggregate sorted DDS/LOD digest receipts where
captured, in addition to the older cook metadata. Coverage is explicit for each
identity field. Missing historical identities are never backfilled from current
assets. Three-launch p95/p99 statistics are descriptive interpolations, not a
reliable estimate of startup tail behavior.

## Exact spatial tile cooking

`octaryn_map_tile_cook` partitions whole triangles by their centroid's 32 m cell,
then enforces 65,536 triangles and 96 MiB of unique texture payload per tile.
Triangles spanning a cell retain their full geometry; manifest bounds contain
actual vertices, not just the nominal cell. A primitive is never merged across
material boundaries. Position, normal, UV0/UV1, tangent and color records are
copied into each GLB. Supported material factors, alpha behavior, samplers and
texture transforms are exported through the same engine model used at runtime.
Unsupported source GLTF features are outside that existing model's contract.

Images and authenticated cooked texture variants live once in shared
content-addressed directories. Each tile references only images it uses. The
manifest's `texture_cache` directs preparation to the shared cache. The optional
`octaryn_map_tiles` build target cooks and verifies Bistro plus per-tile LODs;
ordinary bundles retain the monolithic native reference for comparison.

The first full Bistro cook produced 137 tiles containing all 4,146,017 triangles.
Every tile also passed CPU preparation with the shared cache and 0.5-pixel LOD
selection enabled: zero source decodes, maximum retained geometry/LOD/texture
payload 102,024,172 bytes. This excludes temporary parsing/optimization scratch.
`map_tile_cook --verify source.glb output_directory` round-trips through the real
GLB loader and compares sorted dual 64-bit digests of oriented triangle corner
positions, both UV sets, colors and tangent handedness. This is a conservation
check, not image or streaming-runtime qualification. `validate_map_tiles.py`
also checks material round trips and triangle/texture/count limit rejection.

CPU geometry release is explicit for monolithic maps after the collision owner
copies its mesh. Staged maps release arrays after their upload fence completes;
the collision job already owns its prepared scene. Retained vertex/index counts
allow later BLAS construction without retaining the CPU arrays.

## Bistro build14 runtime checkpoint

Windows DX12 route evidence:
`logs/client/bistro-tiles-build14-route/tiles-dx12-jyuwc2r9/result.json`.
The client exited 0 after 2,220 ready frames. All 137 tile IDs were published, with
137 simultaneously resident at the observed peak, 174 evictions and 8 cancellations.
`map_sampler_cache` reported exactly one live sampler configuration throughout,
exercising the shared cache beyond the old 128-map failure point.

The 879 upload pumps never exceeded 8,388,608 bytes; maximum recorded CPU time was
8.901 ms and 457 pumps exceeded the 1 ms soft target. A separate earlier build14
smoke (`logs/client/bistro-tiles-build14/tiles-dx12-mvsaks33`) peaked at 26.36 ms.
That smoke published 135 IDs with no evictions/cancellations, so it does not
replace the 137-tile route evidence. Driver resource allocation, descriptor/material
preparation and other indivisible calls can overrun the between-operation timer.
The byte cap is enforced; the CPU timer is not a hard per-frame latency guarantee.

Peak tile accounting on the route was 2,130,043,896 bytes under the 2 GiB tile
budget. This is geometry/AS reservations plus unique texture payload, not physical
driver allocation or whole-renderer memory. Two origin-cycle samples recorded
RSS 810,524,672/827,670,528 bytes, renderer allocation estimate 2,160,948,616 bytes,
and DXGI process-local GPU usage 2,326,904,832/2,259,795,968 bytes.

Only two cycles completed; there were no settled-accounting plateau samples and
the final state still had 110 of 137 wanted tiles resident with work pending.
The harness explicitly leaves memory plateau and authority movement unqualified.
Its 384 m renderer-camera route intentionally spends 1,124 frames outside the finite
map and 1,096 inside; empty outside views are not missing-geometry evidence.
The run is a DX12 residency-transition checkpoint, not a Vulkan result, a
30-minute soak, a full gameplay traversal or a frame-performance acceptance.

### Multipart cache hashing checkpoint

`MapTextureHash.cpp` now hashes non-owning spans with CNG or portable incremental
SHA256. Cache keys retain their exact 28-byte option prefix followed by encoded
image bytes. DDS verification and writing hash the existing header and mip
buffers directly, removing the duplicate whole-payload vector. Version 3 metadata,
alpha decisions, texture bytes and asset receipts are unchanged. This removes up
to one 86 MiB temporary payload copy per cache operation; it is not evidence of a
startup speedup or a fix for traversal RSS growth.

`tools/validation/validate_map_hash.py` passed both native CNG and forced portable
implementations, using independent Python hashlib references: 14 input lengths
covering empty, SHA256 padding/block boundaries and a multi-read file; split and
uneven multipart inputs; five material-role cache keys; and two existing small
Bistro RGBA/BC7 assets. Both asset receipts and rewritten DDS bytes match exactly,
and both corrupted copies are rejected. Evidence:
`logs/build/performance-map-hash.log`. This focused CPU fixture does not qualify
the full client build or runtime loading performance.

### Build15 full renderer-camera soak

`logs/client/bistro-tiles-build15-soak/tiles-dx12-b1o7jypn/result.json` passed with
exit 0 after 1,800.00854 seconds and 55 route cycles. The DX12 direct-draw 640x360
renderer-camera route published all 137 Bistro tiles, with 5,542 evictions and 161
cancellations. Maximum upload was 8,388,608 bytes; maximum pump CPU was 19.737 ms.
Peak tile accounting was 2,147,313,860 bytes against the configured 2 GiB budget.
The byte budget passed; indivisible work still exceeds the 1 ms soft CPU target.

The process-memory gate uses 55 per-cycle origin medians, excluding the first 3
cycles. RSS range 44,154,880 bytes and slope 364,051.277 bytes/cycle passed the
128 MiB range/2 MiB-per-cycle limits. Renderer allocation estimate remained exactly
2,160,948,616 bytes. DXGI process GPU usage ranged 2,259,468,288–2,326,577,152 bytes:
64 MiB range and 615,914.188 bytes/cycle passed the 64 MiB/1 MiB-per-cycle limits.
Earlier rising interim samples subsequently levelled off; they do not establish
a leak. The result explicitly reports `process_memory.qualified=true`.

The older, distinct settled-tile-accounting gate remains false because its
sample list is empty (`evidence.memory_plateau_qualified=false`). This does not
negate the completed per-cycle process-memory gate, and must not be relabeled as
settled tile-accounting evidence. The 384m renderer-only route intentionally has
31,265 outside-map and 23,284 inside-map frames. It does not qualify authoritative
Bistro movement,1440p/4K performance, Vulkan, or complete material image quality.
The separate synthetic authority/transport soak has its own evidence. Build15
also contained idle movement drift; build16's native support fix has independent
CPU evidence and the short canonical runtime qualification below.

### Build16 DX12 and Vulkan Bistro route and idle checks

Both canonical build16 2,400-frame routes passed with RHI validation, exit0 and
all 137 tile IDs published:

| Backend and evidence directory under `logs/client/build16-core` | Evictions | Cancellations | Max upload bytes | Max upload CPU ms |
|---|---:|---:|---:|---:|
| DX12: `bistro-dx12/tiles-dx12-aa6ylsqc` | 161 | 6 | 8,388,608 | 72.718 |
| Vulkan: `bistro-vulkan/tiles-vulkan-ag1jzv24` | 155 | 10 | 8,388,608 | 46.256 |

Each actual capture recorded 137 wanted/resident tiles, zero preparation and
zero uploading; both captures were inspected by the coordinator. Each
`idle-qualification.json` reports exactly zero range on all three authoritative
position axes for the 2,280 logged frames after excluding the first 120. This
qualifies the short zero-input Bistro idle case for each named backend/build.

RHI validation and concurrent headless host correctness probes invalidate FPS
qualification here. The large indivisible upload times are recorded, not hidden
by the 1 ms soft target. Neither short route establishes a process-memory plateau
or authoritative player traversal. Vulkan's 30-minute Bistro soak remains open;
the build15 DX12 long-soak evidence is separate.

### Build17 native1440 meshlet comparison

`logs/client/build17-asset-qualification/meshlet-timing/matrix-summary.json`
contains three matched repetitions each of original-ray direct, adaptive-ray
direct and adaptive-ray meshlet rendering on DX12/RX9070XT. All nine runs have
complete cooked receipt coverage and matching schema3 build/asset/settings
identities. Observed camera records match exactly. Available image light records
match; uncaptured repetitions have no direct light sample. The comparison uses
348 common ready frames120–479 after removing capture-adjacent frames.

| Statistic, ms | Original-ray direct | Adaptive direct | Adaptive meshlet |
|---|---:|---:|---:|
| GPU mean |31.87655|17.69362|17.74970|
| GPU median |32.81848|18.35890|18.43702|
| GPU p95 |42.46140|23.32547|23.35858|
| GPU p99 |44.77809|26.28830|26.32957|
| GPU worst |75.67924|29.75788|29.69768|
| CPU frame wall mean |32.05968|17.90056|17.96739|
| CPU frame wall p99 |45.67884|26.49483|27.70699|
| CPU frame wall worst |76.03030|43.60500|55.30530|
| Opaque GPU mean |1.27414|1.27326|1.37962|
| Encode CPU wall mean |0.50578|0.47718|0.25647|

Mean/median/percentiles are medians of run-level statistics; worst is maximum
across runs. Meshlet startup and actual draw markers occur in all three logs:
32,792 meshlets,4,012,107 opaque triangles,one CPU opaque submission; blended
geometry retains forward submission. Meshlet buffers add33,756,376 bytes,
increasing geometry422,002,280→455,758,656 bytes. Process DXGI usage remained
3,323,342,848 bytes in the sampled windows, showing why logical allocation and
driver usage are separate measures.

Meshlets reduce measured encode CPU wall cost but increase opaque GPU cost.
Total GPU mean is0.056078ms slower than adaptive direct (0.317%); paired slowdowns
are0.071432,0.047780 and0.063572ms, versus0.034436ms maximum observed run-mean
spread. There is no winning end-to-end result, so keep meshlets opt-in. The
original-ray→adaptive reduction is14.18293ms (44.49%), distinct from geometry
submission, and still does not meet the16.7ms native milestone.

Actual first-repetition adaptive-direct/meshlet images were inspected; no gross
missing geometry was apparent. These production-sampling images are not exact
fixed-sampling parity evidence. This result does not select defaults for Vulkan,
other hardware,4K,static scenes or other content, nor qualify full visual motion.
Review summary:`logs/client/build17-asset-qualification/asset-review.json`.


### Build38 original-order299 versus monolithic static pilot

The fully resident299-tile layout is **not promoted as a rendering-speed
optimization**. On the same build38 DX12/RX9070XT bundle, retained reflection
path, direct indexed geometry, no LOD, lossless textures and locked post-cut
camera, the single quiet pair was slower with tiles. Both output2560×1440 with
fixed1280×720 internal rendering/FSR. Only ready frames3600–4199 are measured:
600 samples each, renderer frames3951–4550 monolithic and3631–4230 tiled.
An earlier proof capture is outside each selected window; the whole process is
not capture-free. The tiled proof contains299 ready collision/BLAS tiles, stable
resident generation and no subsequent observed upload/AS activity; selected GPU
records explicitly cover external AS and report zero external submissions.

| Exact-window statistic, ms | Monolithic | Original-order299 |
|---|---:|---:|
| GPU mean |6.29995|8.11424|
| GPU median |6.74762|8.80748|
| GPU p95 |7.48153|9.67853|
| GPU p99 |7.62419|9.88848|
| GPU worst |7.69520|10.00692|
| Sun tracing mean |1.37349|1.70980|
| Reflection tracing mean |3.26127|4.69757|

The measured GPU median increases30.53%. This is one pair, with no three-run
noise estimate; its negative direction is sufficient to stop this speed-promotion
pilot, not to assign a universal regression percentage. No Vulkan, traversal,
streaming-movement or repeated-run result is inferred. Existing tiled streaming
remains independently useful for bounded residency; this result does not remove
that path. Geometry partitioning also changes instances and raster submission,
so it does not isolate BVH traversal cost. Shader query counters were disabled;
actual query-count parity and a tests-per-query denominator are unmeasured.

A separate native2560×1440 fixed-sampling/RHI-validation pair at ready frames
3600/3624/3648 matched camera, FOV, lighting and complete ray coverage. All six
images and the largest-difference crop were reviewed: no discernible gross
geometry/material loss in this street view. RGB MAE is0.00312–0.00323 code values,
p99 difference0, maximum11/255. This is selected static-view parity, not motion,
ghosting or full-material acceptance. All4,146,017 oriented triangle
position/UV/color/handedness/material/image keys match across163 material
signatures. Raw normals/tangents are not bit-exact: maximum component differences
1.1920929e-7/1.78813934e-7 exactly reproduce an extra loader normalization; the
strict raw checker remains failed. Pixel differences are not attributed solely
to that normalization.

Post-capture verification checked2,076 listed build/helper/geometry/metadata and
payload identities. Both272-file DDS sets were read and verified against their
receipts and each other:1,320,038,704 bytes including headers per cache. These
reads warm caches; no cold-loading claim is made. Logical4GiB tile admission and
observed OS-budget availability are separate evidence. Periodic tile records
cannot prove unlogged work absent, and full299 residency does not establish
initial-playable time. Evidence and exact frame lists:
`logs/client/hq200-build38/geometry/analysis/{quality,timing}-comparison.json`;
preflight identities under the adjacent `preflight/` directory. The ignored
plan `logs/tools/monolithic-299-static-qualification.json` is complete with
speed promotion rejected; no triplets are scheduled for this losing pilot.
