# Rendering evidence review — 2026-09-27

This is a review of actual captured pixels and their recorded inputs. It is not
full HQ or continuous-motion acceptance. No GPU runs were launched by the reviewer.

## Build14 matched native-AA sequence

The six cases under `logs/client/build14-ray-smokes/{dx12,vulkan}-{reference,adaptive,search}`
contain eight 640×360 captures each: ready frames 180, 204, 228, 252, 276, 300, 324,
and 348. These cover movement, settling, a cut, and a later post-cut frame.
The search variant is reflection-only; the failed shadow-search experiment was removed.

All 48 images were inspected in unscaled contact sheets:

- `logs/client/owner-ray-review/dx12-sequence-{1,2}.png`
- `logs/client/owner-ray-review/vulkan-sequence-{1,2}.png`

Actual camera, lighting, map and dimensions match across compared paths. Recorded
jitter, shadow sequence and reflection sequence indices also match at every
captured frame. Fixed sampling uses ready-frame indices and a fixed presentation
delta; these captures do not qualify rendering speed.

The visible street, hedge/tree silhouettes, bollards, shop frontage and pavement
remain present across the sampled frames. No gross missing geometry, broad light
leak, or detached trail persisting into the sampled post-cut frame was apparent.
Small tonal differences remain. This does not establish absence of short-lived
ghosting, shimmer, noise or thin-feature loss between the 24-frame-spaced samples.
The view does not establish close inspection of glass or interior chrome.

Pixel differences below are diagnostics, not acceptance thresholds. Values use
8-bit RGB code values and compare each adaptive path to its backend's reference.

| Backend/path | Frame RGB MAE range | Frame 99th-percentile channel difference |
|---|---:|---:|
| DX12 adaptive | 0.703–1.324 | 8–14 |
| DX12 reflection search | 0.613–1.287 | 7–13 |
| Vulkan adaptive | 0.705–1.322 | 8–14 |
| Vulkan reflection search | 0.616–1.287 | 7–13 |

Inputs, phase comparisons and per-frame metrics are recorded in
`logs/client/owner-ray-review/build14-sequence-review.json`.

## Build10 native-resolution stills

The first saved motion-workload captures were inspected for these pairs under
`logs/client/performance-build10-full`:

| Workload | Reference case | Adaptive case |
|---|---|---|
| DX12 native 1440p | `map-dx12-on-0-snc3z0iv` | `map-dx12-on-0-5tymm388` |
| DX12 native 4K | `map-dx12-on-0-93hpe6qz` | `map-dx12-on-0-k42vcyxt` |

The `review.png` files are lossless conversions of `frame.bmp`. The displayed
overviews preserve the same street objects and vegetation silhouettes without a
gross discrepancy. Display resizing limits this check: it does not qualify native
pixel detail, temporal stability, or the full movement sequence. Build10 does not
validate changes made afterward.

## Candidate material views

The geometry-only audit of `map_quality_views.py` used baked GLB primitive AABBs,
a 90-degree vertical field of view and a 16:9 aspect ratio. Conservative frustum
intersection retained 4/7 leaf and 8/22 hedge bounds in `foliage`, 3/4 exterior-glass
bounds in `shop_glass`, 2/2 Bistro-sign and 5/11 banner-metal bounds in `signs_metal`,
and 3/3 chrome bounds in `chrome_interior`. Bounds intersection does not establish
visible triangles, lack of occlusion, a useful reflection angle or sufficient
screen coverage. Actual captures below expose a material-coverage failure that the
bounds check could not detect.

## Build16 DX12 native 1440p material sequence

All eight frames in each reference/adaptive pair under
`logs/client/build16-core/quality-dx12/quality-di4obxfc` were inspected in paired
overview/crop sheets. These are native rendering with upscaler mode 0, fixed
sampling, and no reflection-search or sparse flag. Actual camera, lighting, map,
dimensions and all recorded sampling phases match across each pair.
`logs/client/owner-ray-review/build16-dx12-review.json` records the comparisons.

| View | Visible coverage and limits |
|---|---|
| `foliage` | Trees, hedge leaves, fine silhouettes, pavement and facade. Useful vegetation coverage. The central doorway remains nearly black and speckled in both reference and adaptive. |
| `shop_glass` | Close green shopfront and large dark panes, colored light details, foreground bottle and tables. Both paths have dark, speckled panes; this is no proof of physically correct transmission or reflections. |
| `signs_metal` | Camera is inside/against a red interior pillar, looking at ceiling and part of the restaurant. This view **fails to qualify exterior glossy signs or metal** despite its material-bound intersections. |
| `chrome_interior` | Restaurant furniture, plates, stemware, cutlery and ceiling lights. Stemware looks opaque gray and cutlery nearly black in both paths. There is no clear high-contrast chrome reflection to qualify sharp reflected detail. |

Unscaled first-frame crops for doorway, shop panes and interior tableware were
also inspected. They confirm that the dark/flat appearance exists in reference;
the comparison does not establish its underlying cause or correctness. No gross
geometry disappearance or detached trail persisting into the sampled post-cut
frame was apparent. The overview sheets reduce the full frame to 320×180, and
the sequence skips 23 frames between captures: this does not establish fine
detail preservation, absence of transient ghosting, or continuous-motion quality.

Full-frame RGB MAE ranges, in 8-bit code values, are 0.169–1.366 for foliage,
0.058–0.835 for shop glass, 0.002–0.154 for the mispositioned signs view, and
0.008–0.104 for the interior. These are descriptive diagnostics, not acceptance
thresholds; low error in an unhelpful view cannot qualify the intended material.

The first build16 Vulkan material run failed its unchanged frame watchdog before
authoritative-player readiness:
`quality-vulkan/quality-tjg6zfiu/foliage/reference/map-vulkan-on-0-4wuvayhd`.
Its failed evidence remains preserved. It supplies no successful Vulkan material
sequence or visual acceptance. The unchanged-watchdog traced reproduction
`logs/client/build16-vulkan-startup-trace/map-vulkan-on-0-ivoum78c` and fresh
foliage/shop-glass suite `logs/client/build16-vulkan-quality-recheck/quality-l3krfv2j`
subsequently passed. Those passes do not explain or repair the original failure.

The failed run's frame 39 spent 745.287 ms in rendering with 15.625 ms of main
thread CPU execution. Frame 40 took 467.703 ms overall, with 7.257 ms rendering,
94.962 ms cap sleep and zero measured main-thread CPU execution. This supports
waiting/descheduling rather than 745 ms of CPU computation, but the original
menu path lacked stage tracing, so no individual API or driver cause is proven.
An earlier forward pipeline creation took 1308.9 ms with a cache miss, versus
1.5 ms cached in the passing reproduction; that happened before map loading
completed and frame 0, so it cannot be assigned as the cause of frame 39.
Build17 adds opt-in menu stage intervals; this is diagnostic coverage, not a
stall fix. Its separate 360-frame traced reproduction also passed unchanged
watchdog in `logs/client/build17-menu-trace/map-vulkan-on-0-1ttbukfq`.

The successful Vulkan recheck's 32 images were subsequently inspected in paired
overview/crop sheets (`owner-ray-review/build16-vulkan-{foliage,shop_glass}-{1,2}.png`).
All 16 reference/adaptive frame pairings match actual inputs and jitter/shadow/
reflection phases. Trees, hedge silhouettes, shop framing and foreground objects
remain present. Dark, speckled panes are visible in both paths; no gross missing
geometry or trail persisting into the sampled post-cut image was apparent.
The same reduced-overview and 24-frame sampling limitations apply. RGB MAE ranges
are 0.169–1.366 for foliage and 0.060–0.836 for shop glass, recorded in
`logs/client/owner-ray-review/build16-vulkan-review.json`. These are descriptive,
not quality thresholds or evidence that the original startup failure is fixed.

## Corrected material camera candidates

The revised exterior sign eye is `[-5.1,6.0,0.1]`, targeting
`[-2.24,5.99,-0.95]`. CPU first-hit rays from the motion origin, end and cut
positions hit `Bistro_Sign_Letters`; the straight camera translation segment
crosses no triangle. The actual root-reviewed build17 preview
`logs/client/build17-material-previews/signs_metal/map-dx12-on-0-ihlgui1k`
shows the exterior lettered sign and metal frame above its red awning.
This replaces the obstructed candidate.

The revised chrome eye is `[5,1.9,-9.8]`, targeting
`[5.5996,1.9125,-10.9533]`. CPU center rays from the origin/end/cut positions
first hit `Metal_Chrome1`, and the translation segment has no triangle crossing.
The root-reviewed preview
`logs/client/build17-chrome-opposite-preview/map-dx12-on-0-zh9xf1w2`
shows the wine-glass rack/bar with thin dark metal and white specular glints,
plus gray stemware and plates. This establishes actual chrome-rack coverage;
it does not establish broad mirror-sharp reflection quality. The earlier eye
`[6.2,1.9,-12.1]` was blocked by wooden counter geometry and is rejected.

GLB PBR factors are implicit 1.0. The metallic-roughness images are constant
16×16 textures: `Bistro_Sign_Main` has roughness 188/255 and metallic 0;
`Banner_Metal` has roughness 188/255 and metallic 1; `Metal_Chrome1` has roughness
89/255 and metallic 1; `Cutlery_chrome` has roughness 79/255 and metallic 1.
Thus the named chrome is moderately rough, not a mirror-sharp reflection test.
`MASTER_Glass_Exterior` has roughness 0 and metallic 0. These values describe
actual source channels, not an inference from material names.

## Tiled-world capture and idle evidence

Build15 Bistro case `logs/client/bistro-tiles-build15-route/tiles-dx12-pvqkmur0`
captured render frame 679 / ready frame 654 during the initial hold. Its metadata
reports all 137 wanted tiles resident, no preparation/upload work, complete ray
coverage and scene revision 137. The image follows the renderer's 64-frame
convergence requirement. This short route does not qualify a memory plateau.

Build16 DX12 case `logs/client/build16-core/bistro-dx12/tiles-dx12-aa6ylsqc`
also has a real capture. Its `idle-qualification.json` reports exactly zero range
for all three authority-position axes over 2,280 rows after the first 120 rows.
That is short production evidence for the idle-contact repair; native 108,000-step
slope/support regressions are separately recorded in
`logs/client/owner-ray-review/idle-support-native.json`.

Moving-item rendering remains outside these image checks: an authoritative item
workload does not substitute for a connected item-pose visual contract. Windows
DX12/Vulkan results do not qualify other operating systems or Metal runtime.
