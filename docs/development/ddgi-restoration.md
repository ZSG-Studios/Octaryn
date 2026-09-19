# DDGI restoration — 2026-09-18

Subsequent requested corrections to stale updates are recorded in
[DDGI live response](ddgi-live-response.md). The reference-match checks below
describe the restoration snapshot before those corrections.

The user explicitly reversed the Split Radiance Cascades replacement and requests
complete removal of that renderer and restoration of the previous DDGI system,
including settings, debug views and runtime behavior.

## Source of truth

`d554b64` is the last repository commit before the SRC integration (`77ea3d1`).
The latter also introduced independent DDGI response repairs; the latest repaired
DDGI implementation survives in the current committed tree (`6df3659`), before
the uncommitted SRC-only cutover. Restoration uses those repaired DDGI owners,
compares the original settings/API with `d554b64`, and preserves unrelated startup,
fence, temporal, streaming and gameplay repairs.

The mesh-independent hierarchical voxel tracer and far-field foundation were
introduced solely for the withdrawn renderer and have no other runtime consumers.
They are removed with it. Existing hardware voxel acceleration structures and
ray-query shaders used by DDGI, direct lighting and reflections are retained.

## Settings

The original F6 GI radius control (0–32, zero disables fine GI), coarse range
(0–1024), all 31 debug IDs/names and quality levels 0–3 are restored. Defaults
remain fine radius 6, coarse radius 128 and High quality (2). The saved user file
already requests fine radius 16, coarse radius 128 and quality 3, so it was not
overwritten. Its radius-4 view, resolution and upscaler/pacing preferences remain.
Requested lighting defaults remain ambient 0.65, sun 0.75, fog 1024 and floor 0.25.

## Completed restoration

- Restored DDGI allocation, independent coarse/fine volumes, occupancy,
  scheduling, seeding, tracing, relocation, updates and HDR/fluid sampling.
- Restored genuine probe captures, timing, overlays and debug composition.
- Removed the replacement renderer, its specialized tracer/far-field foundation,
  associated runtime hooks, shaders, tools, build targets and active plans.
- Preserved startup responsiveness, bounded fences/watchdogs, frame pacing,
  prediction, inventory, atlas and temporal/shadow fixes.
- All 17 DDGI backend/shader files and the ray-traced HDR composite match the
  repaired committed reference after line-ending normalization. Evidence:
  `logs/client/ddgi-restoration/source-parity.json`.

## Verification

| Area | Executed result |
| --- | --- |
| Windows native client bundle | Passed; `logs/build/ddgi-restored-bundle.log` |
| Packaged shader contents | Source/bundle equality passed; no removed shaders left in the active package |
| Shader compilation | 77 cases per DXIL, SPIR-V and Metal target passed; `logs/build/ddgi-restored-shaders-*.log` |
| DDGI scheduler | 35 regression cases passed |
| UI contracts | 701 general, 59 FSR and 1,723 inventory checks passed; DDGI range control and full debug cycle included |
| DDGI volume sampling | DX12 and Vulkan passed recursive order, constant energy, boundary continuity, pending handoff, solid occlusion and dark-field cases |
| Production lighting graph | DX12 and Vulkan each completed 600 world frames / 81 columns, actual probe readbacks and nonzero DDGI trace/update timestamps |
| Production volume settings | Fine-only, coarse-only and both-off captures match requested radii, allocation flags and counts; both-off has zero probes and zero DDGI GPU timing |
| Moving saved user view | Eight captures at 2560x1440 Native AA per backend; both sequences inspected, zero temporal resets |

Evidence is under `logs/client/ddgi-restoration/`. Main production cases:

- DX12: `lighting-dx12-high-581io_el`
- Vulkan: `lighting-vulkan-high-ugpcdo8k`
- DX12 saved view: `visual-dx12-0-native-uvz8318l`
- Vulkan saved view: `visual-vulkan-0-native-j48ntz1o`
- Volume settings: `volume-settings.json`, with captures under each mode's
  `verified/` directory.

Both authored-scene screenshots and natural-world motion montages were inspected.
The natural view again has lit terrain/foliage instead of the withdrawn renderer's
dark appearance. Saved fine radius 16/coarse radius 128 produces 32,768 fine and
12,288 coarse probes; both volumes contain valid irradiance in actual readbacks.

### Saved-view measurements

| Windows RX 9070 XT, 2560x1440 Native AA | DX12 | Vulkan |
| --- | ---: | ---: |
| DDGI trace + update median | 0.246 ms | 0.253 ms |
| Frame wall median | 3.747 ms | 3.721 ms |
| Frame wall p95 | 6.444 ms | 4.961 ms |

These are explicit hidden, input-disabled moving-camera qualification runs;
frame-wall measurements exclude synchronous capture frames. They are not a
guarantee of ordinary VSync-limited FPS, streaming performance or other scenes.

### Qualification harness corrections

The standalone volume fixture initially passed sampling but failed shutdown:
it acquired a queue without initializing the production frame-completion fence.
The fixture now initializes that fence and both backend reruns exit zero.
Production shutdown safety was not weakened. Shader staging no longer depends
on Vulkan validation files; explicit Vulkan validation targets retain that dependency.

The first three settings experiments used the generic benchmark's default settings,
so they did not establish independent volume modes. The saved-view capture runner
now passes `--benchmark-settings` and verifies captured requested radii. Only the
corrected `verified/` captures count as volume-setting evidence. Moving runs use
the separate frame/UI-validation path and already loaded the intended settings.

## Scope of the result

The previous DDGI implementation and controls are restored, rather than replaced
with a different lighting algorithm. Existing DDGI convergence limitations in
`ddgi-response-repair.md` and `lighting-response-repair.md` are not claimed fixed.
Windows runtime results do not qualify Linux Vulkan or macOS Metal. Networking
changes were not part of this restoration; the local bundled-server path ran in
the production-client checks.
