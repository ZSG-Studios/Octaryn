# OpenFNV interior visibility qualification

Recorded 2026-10-01. The executable in this evidence uses the current Slang-RHI
renderer. This work does not qualify the planned NVRHI migration.

## Observed camera problem

The interactive Novac session `OpenFNV/logs/interactive-novac-20261001-072941`
reported an authoritative eye at `(0, 2.840, 1.300)`. Its map manifest spawn was
`(0, 0.5, 1.3)`. The authoritative player camera adds its body/eye offset; this
placed the eye above the imported room ceiling. The actual elevated capture
shows a roofless room viewed from outside. The authored single-sided ceiling
correctly culls its backfaces. An interior capture at eye height `0.5` shows
the ceiling, walls and floor.

An explicit `OCTARYN_CLIENT_SCENE_PREVIEW_CAMERA=x,y,z,yaw,pitch` now requests an
authoring camera independently of the server player pose. It suppresses player
movement and jump inputs and changes only the rendered camera. It does not
adapt gameplay units, character dimensions, collision, saving or simulation.
Its absence retains the gameplay camera. This is preview tooling, not a retail
gameplay camera or a complete flicker fix.

## Captured baseline

The three baseline receipts are under the sibling `OpenFNV/logs/` directory.
They used the same packaged glTF SHA-256:
`9dc01b2a4e88e5c0288b710fc536cc19a1b01dca9a751bd841728623462d855f`.

| Receipt directory | Eye | Captures | Observation |
| --- | --- | --- | --- |
| `flicker-baseline-static-v1` | `(0,0.5,1.3)` | 8 consecutive | All pixels identical; complete interior visible |
| `flicker-baseline-eye-v1` | `(0,2.84,1.3)` | 8 consecutive | All pixels identical; ceiling viewed from outside |
| `flicker-baseline-sweep-v1` | Starts at `(0,2.84,1.3)` | 12, stride 16 | Translation/rotation, settle and camera cut; settled and post-cut samples separately identical |

All use DX12, 960x540, ray tracing off, upscaling off, required
virtual geometry, a 384 MiB requested geometry pool, a one-pixel geometry error
budget, hidden capped execution and unchanged watchdog constraints. Each exits
cleanly. The two static sequences use fixed sampling; the moving sweep uses
normal sampling. They are visual evidence; capture readbacks exclude performance claims.
The elevated sweep does not qualify motion inside the room.

Interactive logs show residency stabilizing at 85 pages, zero pending pages,
zero feedback overflow and no RHI validation errors. History occlusion remains
disabled by default. Visibility ties use cooked cluster identities rather than
variable selection append order. The DX12 backend automatically emits UAV
barriers between dependent unordered-access bindings. No speculative renderer
barrier, culling, LOD, depth or quality changes were justified by this evidence.

## Authored camera checks

Run from the engine root:

```powershell
python tools/validation/check_scene_preview_camera.py
```

The script compiles the portable native fixture using the existing Visual Studio
environment and clang-cl, then executes it directly. It uses no GPU or CTest.
The receipt is `build/windows-x64/tools/scene-preview-camera/result.json`, with
compiler/test output in `checks.log`. The current run passes 35 assertions:
authored origin independent of player pose; retained projection and jitter;
yaw/pitch/vertical movement; normalized pitched diagonal speed; sprint and
elapsed-time bounds; invalid origin rejection; suppression of all movement and
jump events; and unchanged disabled behavior. The diagonal check exposed and
verified correction of normalization in world space.

## Subsequent movement defect and correction

The user's manual movement still exposed disappearing meshes with all 85 pages
resident. The small cut-only observation below did not explain that defect.
`OcclusionGpu::begin` classified current early bins but supplied older indirect
arguments until `retest`, after their first use. Finalizing those arguments
immediately after classification corrects the missing cluster dispatches.
A focused GPU fixture fails on the old implementation and passes after the fix.

The selected source package is now `OpenFNV/generated/novac-source-preview-v3`,
which also corrects the independently verified NIF node-matrix transpose error.
The fan's source bytes and placement stay unchanged; its source child rotation
now renders horizontally. The rebuilt viewer completes 650 hidden frames and
16 full-turn captures in `OpenFNV/logs/source-cell-loop-after`. Eight repeated
poses yield identical images; all 85 pages remain resident through the warmed
sequence with zero selection error, requests, missing roots or overflow.

See [virtual-geometry-bin-count-stability.md](virtual-geometry-bin-count-stability.md)
for before/after GPU evidence, preserved failed baseline captures and exact limits.
The correction changes ordering, without changing raster quality or watchdogs.

## Earlier limited captures and remaining source gaps

The earlier source-retaining package was `OpenFNV/generated/novac-source-preview-v2`.
The canonical `octaryn_all` build passed. `OpenFNV/logs/source-cell-static-final`
loads the actual game inventory (126 records and 91 dependencies), activates the
independent preview camera and renders 34,182 triangles/180 primitives/107 images.
It exits after 250 hidden frames, with eight consecutive pixel-identical
interior captures. Marker visualization meshes are no longer active; original
helper records and meshes remain retained.

`OpenFNV/logs/source-cell-sweep-final` runs 430 frames with the same watchdog,
native DX12 and quality settings. Settle frames 291/307/323 match, and post-cut
frames 355/371 match. First-cut frame 339 differs from frame 355 at 903 pixels
in bounds `(211,208)-(569,278)`, around shelf/door edges, despite identical poses.
Its maximum channel delta is 127. Eight captures at the exact cut pose in
`OpenFNV/logs/source-cell-cut-pose-static` all match the post-cut sweep image
byte-for-byte and have zero selection errors. Temporal mode is inactive and
jitter is zero in both cut observations. Residency grows from 75 to 78 to 81
pages across the sweep without eviction; the matched static run uses 79 pages.
This supports an inference of demand-loaded coarse-to-fine detail replacement.
Per-cut residency telemetry has not established the exact changed cluster, so
this is not a proof of the cause or absence of other interactive flicker.

The old package had 63 opaque, 26 masked and 22 blended material primitives.
Some authored NoLighting texture inputs were missing in that baseline package.
Wallpaper/water-stain decals preserve source decal flags, but the generic glTF
forward path uses ordinary depth testing and primitive-distance transparency
sorting. Source decal depth bias and blend semantics are not qualified by these
captures. Global bias, double-sided rendering, disabled culling or forced full
detail would hide symptoms and were not introduced.

The source cell is a partial static material preview with explicit unsupported
placements and camera-dependent geometry. No conclusion here establishes full
New Vegas material/lighting parity, complete cell gameplay, absence of flicker
at every camera position, movement performance, other GPU vendors or Vulkan.
The camera is integrated and exercised by those fresh interior captures. Manual
interactive movement has not been injected or claimed tested. The matched
measurements do not qualify gameplay player dimensions or original Havok behavior.
