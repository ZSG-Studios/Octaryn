# Remaining GPU qualification candidates

Prepared for the coordinator after the canonical build15 checkpoint. None of
the commands below was launched while preparing this document. Run sequentially
on one unchanged bundle; no builds, cooking or other GPU work during timing.
Preserve failed cases. DX12 results never qualify Vulkan. Begin each timing set
only after correctness and watchdog smokes pass for that exact bundle.

## Engine shader/pipeline cache startup: six processes per backend

```powershell
python tools/validation/capture_shader_cache_pairs.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/build15-cache-startup-dx12 --backend dx12
```

Three independent empty/reuse pairs; emitted `report.json` and `report.md` retain
exact identities, prelaunch cache inventories, startup/ready times and cache
hits/misses/writes. Repeat with `--backend vulkan` and a distinct evidence root.
This controls only the engine cache. It is not OS/driver/filesystem-cold evidence
and never qualifies engine FPS. Details: [shader-cache-startup.md](shader-cache-startup.md).

## Safe BC7: inspect quality before timing

```powershell
python tools/validation/capture_asset_variants.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/build15-bc7-quality-dx12 --backend dx12 --size 1440p --mode 0 --phase quality
python tools/validation/capture_asset_variants.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/build15-bc7-timing-dx12 --backend dx12 --size 1440p --mode 0 --workload motion --phase timing
```

First command runs eight material-view/codec sequences, then verifies actual
camera/light/jitter/ray phases and produces pixel comparisons. Inspect native
frames and motion before proceeding to the six-process timing command. Both
retain identical full-detail direct raster and RT reference sampling. Only the
existing safe opaque-color BC7 assets differ; no recook or hardlink operation.
Accepting the codec still needs static, 4K, separate reconstructed-mode and Vulkan
checks; use distinct evidence roots and the corresponding `--workload`, `--size`,
`--mode` and `--backend` values. Do not average those workloads together.

## Meshlet selection: nine matched runs for one route

```powershell
python tools/validation/performance_matrix.py --bundle build/release-windows/client/bundle --output logs/client/build15-meshlet-dx12-1440 --backend dx12 --sizes 1440p --modes 0 --variants reference adaptive meshlet --workloads motion
python tools/validation/summarize_performance_matrix.py logs/client/build15-meshlet-dx12-1440
```

The adaptive direct path is the meshlet comparison baseline; reference rays are
included because the existing matrix contract requires them. Three repetitions
per variant retain full geometry and no LOD. Require meshlet activation in actual
logs, exact asset/camera/light/build identity, no visual loss, and a gain beyond
observed run spread. Repeat independent Vulkan, static and 4K cases before any
capability-tier default selection. First-repetition images use production sampling
and do not replace deterministic quality-only parity checks.

## Higher-resolution UI pooling/cropping: eight cases per resolution

```powershell
python tools/validation/capture_ui_effects.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/build15-ui4k-cropped --backend all --surface all --width 3840 --height 2160
python tools/validation/capture_ui_effects.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/build15-ui4k-reference --backend all --surface all --width 3840 --height 2160 --full-size-surfaces
```

The commands each print their generated `run-*` directory. Pass those exact two
directories to `compare_ui_filters.py --cropped <cropped-run> --reference <reference-run>
--output logs/client/build15-ui4k-parity.json`. Require exact RGBA parity and inspect
menu/card captures, allocation counters, blur/shadow layers and rounded clipping.
Repeat at 2560x1440 if that resolution is required separately. These diagnostic
captures exercise real RHI validation and are not UI FPS evidence. Vulkan requires
the installed validation SDK; do not silently skip a failed backend.

## Independent gates not replaced by this queue

Full graphical Bistro memory plateau/30-minute traversal, Vulkan Bistro residency,
final-binary map switching, network reconnect and material-focused RT quality
remain distinct. The build14 DX12 route proves 137-tile publication and residency
transitions only. Keep protocol/build changes outside every paired measurement
window and append exact result paths to [performance-acceptance.md](performance-acceptance.md).
