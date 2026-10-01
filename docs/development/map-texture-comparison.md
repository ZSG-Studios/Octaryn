# Lossless and opaque-color BC7 qualification

The safe candidate already exists. Use
`build/release-windows/client/map-variants-opaque/variants.json`; do not recook,
relink or overwrite the default lossless bundle during measurements. The older
`map-variants` candidate compressed alpha-bearing textures and is rejected.

`capture_asset_variants.py` uses those two existing manifests and emits a plan,
isolated cases and separate timing/quality reports. It checks their authenticated
DDS receipt identities before launch, then checks captured GLB, LOD, executable,
shader, driver, settings and actual camera/light evidence. Only the declared
texture payloads and cooker metadata may differ. Both codecs retain direct,
full-detail raster geometry and the same RT reference sampling path; codec names
remain `lossless` and `bc7` throughout the reports.

Preview all commands without launching a process:

```powershell
python tools/validation/capture_asset_variants.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/asset-variant-plans --backend dx12 --size 1440p --mode 0 --phase both --dry-run
```

Once competing GPU work, compilation and cooking have stopped, run a matched
timing set (three fresh processes per codec, alternating order):

```powershell
python tools/validation/capture_asset_variants.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/asset-variants-dx12-1440-motion --backend dx12 --size 1440p --mode 0 --workload motion --phase timing
```

Repeat with `--workload static`, then `--size 4k`. Qualify Vulkan independently
with `--backend vulkan`. Report `--mode 1` (native AA) and `--mode 2`
(reconstruction) separately from mode 0; the report preserves observed render
and output dimensions. Each invocation creates its own directory. Do not combine
different backends, dimensions, modes or workloads into one measured average.

Timing runs are uncapped, with one actual sunlight capture per case. The report
uses identical ready-frame indices and removes capture-adjacent frames, then
reports per-run and three-run CPU/GPU distributions plus sampled RSS/private
memory and texture/GPU byte counters. Startup stages and readiness clocks come
from their actual log markers; overlapping intervals are not summed. Every
launch starts a fresh process, but OS/shader cache state is unknown. The first
launch is never called cold. Missing counters remain unavailable.

Run the image sequence separately on a bundle that implements and records
ready-frame fixed sampling:

```powershell
python tools/validation/capture_asset_variants.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/asset-quality-dx12-1440 --backend dx12 --size 1440p --mode 0 --phase quality
```

This captures eight frames for each codec at foliage, shop glass, signs/metal
and chrome-interior candidates, including movement, settle, cut and post-cut.
Material-bound camera candidates require visible-coverage inspection. Actual
jitter, shadow/reflection sequence, presentation delta, temporal reset and camera
pose must match before pixel comparisons run. `comparisons/<view>` contains the
metrics and lossless/BC7/difference-times-eight sheets; original captures remain
in each case. Inspect native images and motion for coverage changes, glass
opacity, glossy detail, disocclusion, noise and ghosting. Repeat on Vulkan and
at the remaining target resolutions/modes. Fixed-sampling quality runs explicitly
do not qualify FPS. UI and moving-item visual coverage require separate scenes.

The candidate must upload 841,716,572 texture payload bytes versus
1,319,998,448 for lossless, with zero source decodes. Its 141 opaque color variants
use BC7; alpha-bearing color, normal and data variants remain byte-exact lossless.
Opaque BC7 alpha may decode to 254 rather than 255, so inspect materials combining
an opaque texture with blend factors too. CPU image metrics and the 36.23%
payload reduction do not establish visual acceptance or rendering-speed gains.
Keep default lossless assets until paired quality and timing gates pass.

## Build14 DX12 timing result

Evidence: `logs/client/asset-variants-build14/asset-pair-l4v1wjiz/timing.json`.
Recomputing the report from all six raw cases produced an identical result.
Three repetitions per codec used native 2560x1440, direct full-detail geometry,
Ultra RT reference sampling, no LOD/upscaling, fixed camera/light and the motion
route. They were uncapped, below-normal priority, with no RHI validation, ray
counter atomics or fixed-sampling override. The same 276 ready frames 120–399 were
measured after 120 warmup frames, excluding capture-adjacent frames 299–302.

Captured executable hash `764871210aa183a2615fdde276f48d7f3965eb1a8848612ac99ac6b20b835359`,
shader/server hashes, source GLB, LOD receipt, settings, adapter/driver and actual
camera/FOV/light records matched. RX 9070 XT driver was 32.0.31041.1004. Each codec
has 3/3 manifest, metadata and aggregate DDS/LOD receipt coverage. The only
permitted asset difference was its declared texture payload/cook metadata.
Every run used 272 unique texture uploads across 555 logical variants, sharing 283
duplicates, with zero source decodes. BC7 runs recorded 141 BC7 and 131 RGBA uploads.

The runner serialized the six processes in lossless/BC7, BC7/lossless,
lossless/BC7 order. Saved prelaunch-build and completion artifact windows are
non-overlapping with approximately 0.45–0.46s gaps. These timestamps and the
runner's blocking process calls establish serialization of these six cases;
there is no system-wide trace proving absence of every unrelated background
task. All six shader caches recorded 46 shader hits and 38 pipeline hits, zero
misses/writes/errors. OS file cache state remains unknown, so these are fresh
processes with warm engine shader caches, not cold-loading measurements.

| Startup measurement, median of 3 launches | Lossless ms | BC7 ms | Lossless minus BC7 ms |
|---|---:|---:|---:|
| Images: validated cache reads and GPU texture creation |1971.31|1317.01|654.30|
| Map model loading |613.99|657.99|-44.00|
| Geometry optimization |506.87|483.92|22.95|
| Geometry upload |93.85|94.92|-1.07|
| Material preparation |0.90|1.06|-0.16|
| Map pipelines |116.81|119.06|-2.25|
| Map renderer total |3446.00|2776.10|669.90|
| Collision preparation |1788.90|1768.40|20.50|
| Map worker completion |5331.00|4641.00|690.00|
| Renderer-ready boot milestone |924.00|913.00|11.00|
| Authoritative-ready session elapsed |9342.374|10288.451|-946.077|
| Authoritative-ready SDL uptime |15680.595|16394.780|-714.185|

The image stage improves 33.19% and the map worker completes 690 ms earlier, but
authority-ready medians are later. Therefore this does **not** establish faster
end-to-end launch readiness. Those clock origins differ, startup timings overlap,
and their medians must not be added together. Three startup samples do not
establish reliable p95/p99 launch tails or isolate the readiness difference.

| Rendering statistic | Lossless ms | BC7 ms |
|---|---:|---:|
| GPU mean |32.261593|32.152966|
| GPU median |32.758120|32.649680|
| GPU p95 |42.745210|42.679430|
| GPU p99 |45.044350|45.023850|
| GPU worst |75.626680|75.024240|
| CPU frame wall mean |32.409920|32.317555|
| CPU frame wall median |32.885550|32.739901|
| CPU frame wall p95 |43.124475|43.010249|
| CPU frame wall p99 |45.273351|45.383700|
| CPU frame wall worst |76.476601|75.129105|

Rendering aggregates are medians of run-level statistics; worst is the maximum
across runs. GPU mean decreases 0.108627 ms (0.337%). Paired reductions are
0.044989, 0.108660 and 0.091028 ms. The larger three-run mean range is 0.089211 ms;
the report's criterion passes because the aggregate gain exceeds that range and
all paired reductions are positive. The margin is only 0.019416 ms, so this is a
marginal short-route result, not a major FPS gain or statistical-significance
claim. CPU p99 slightly regresses; the worst frames remain about 75 ms. The opaque
GPU pass mean decreases1.259384→1.195114ms while forward shading is nearly
unchanged. Main-thread execution samples average0.735960→0.905797ms but advance
in coarse Windows quanta; they do not establish a fine-grained CPU regression.

| Memory statistic, median of per-run medians | Lossless MiB | BC7 MiB | Reduction MiB |
|---|---:|---:|---:|
| Texture payload |1258.849|802.723|456.125|
| Renderer allocation estimate |2781.457|2325.332|456.125|
| DXGI process-local GPU usage |3169.387|2593.387|576.000|
| Process resident memory |719.383|717.988|1.395|
| Process peak-resident counter |2033.160|2032.020|1.141|

GPU texture payload drops exactly 478,281,876 B (36.23%). Geometry 422,002,280 B and
AS 378,066,612 B remain identical. The larger DXGI reduction includes driver
allocation behavior and is distinct from logical texture bytes. Steady CPU RSS
is essentially unchanged; peak-resident counters still exceed 2 GiB and the worst
BC7 peak is slightly higher than lossless, so this is not a peak CPU-memory win.

**Visual acceptance was still open for this build14 timing set.** The one production-sampling capture per
timing run validates sunlight/camera identity; it is not the fixed-sampling
foliage/glass/glossy motion suite. Native 4K, reconstructed modes, static routes,
Vulkan and later binaries require their own results. Keep the lossless default
until those quality and scope-specific timing gates pass.

## Build17 native DX12 four-view quality review

`logs/client/build17-asset-qualification/bc7-quality/asset-pair-ekt7tcp_` completed
eight fixed-sampling processes: lossless and safe opaque-color BC7 for foliage,
shop glass, the corrected exterior sign and the opposite-side chrome rack.
Each pair has eight native 2560x1440 captures at ready frames180–348, stride24.
The report verifies exact camera/light, jitter, shadow and reflection phases;
schema3 executable/managed/native/shader/server identities and geometry/settings
match. Only the explicitly allowed cooked texture identity differs.

| View | RGB channel MAE range, codes out of255 | RMS range | Maximum fraction of RGB channels differing by more than16 |
|---|---:|---:|---:|
| Foliage |0.13972–0.14301|0.38651–0.41605|0.001700%|
| Shop glass |0.06692–0.13618|0.27361–0.41574|0.000389%|
| Exterior sign |0.13782–0.15260|0.39929–0.43460|0.000778%|
| Chrome rack |0.05724–0.10051|0.24654–0.35032|0.000362%|

All32 paired frames were reviewed in the four sequence sheets, alongside native
pixel crops of foliage edges, glass framing, sign lettering/ivy and chrome
highlights. No obvious codec-induced foliage coverage loss, missing detail or
new gross artifact was apparent in these samples. Small color/texture differences
remain; metrics are descriptive and are not a perceptual acceptance threshold.
Sheets and native crops are under `review/`; the review record is
`logs/client/build17-asset-qualification/asset-review.json`.

This is scoped visual evidence for the safe codec on these four DX12 views.
Frames sampled every24 intervals do not establish continuous-motion freedom
from short ghosting/flicker. The chrome rack is thin moderately rough metal,
not a broad mirror reflection test. Native4K, reconstruction, Vulkan, longer
motion and broader materials remain open. Keep the lossless default. These
quality-only runs do not update the build14 FPS/timing claims above.
