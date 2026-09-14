# Lighting movement, digging and player shadows

This repair addresses the camera-dependent lighting boundary, sparse/stale
lighting around edits and the missing player shadow in the restored Slang RHI renderer.

The local-light filter described in this historical repair was subsequently
removed. Current source uses [deterministic tiled direct lighting](local-lighting.md)
without local temporal history. The captures and numerical results below remain
evidence for their recorded payload, not the replacement.

## Changes

- GI fades follow continuous camera position instead of jumping at integer probe
  grid boundaries. A 12x12x12 voxel-center cascade at one-block spacing provides
  samples inside narrow passages and at nearby ceilings, alongside the existing
  four-block coarse volume. The two volumes use 4,809,728 bytes and schedule at
  most 14,336 primary rays per frame at defaults.
- Geometry edits request GI recovery when replacement acceleration structures
  publish. Four reactive updates retain prior irradiance while responding faster;
  startup averaging and reserved new-cell scheduling avoid stale or starved cells.
  The actual scheduler covers the fine volume within 27 frames after an edit.
- Static geometry visibility uses 64 fixed rays per probe independently of the
  48 rotating lighting rays. Visibility moments no longer change solely because
  lighting sample directions rotate. Mature irradiance limits isolated bright
  deltas and retains smoothing on ordinary dark samples; real edits retain their
  four-update response window.
- History weights account for how frequently each probe actually updates.
  Nearby probes no longer become noisier because the scheduler retraces them
  every frame. Valid geometry classification/relocation stays fixed until a
  scene publication requests revalidation.
- Local sampled radiance has bilinear surface-validated temporal reconstruction,
  neighborhood history clamping and two edge-aware spatial passes. Diffuse color
  is factored out to preserve texture detail; glossy surfaces avoid spatial blur.
  Histories reset on resolution changes, camera cuts and light-list changes.
- GPU skinning drives full-body player ray and raster shadows, including the
  body hidden by the first-person camera. Sun history clamps stale visibility
  around moving occluders. This does not add deferred GI reception for the player.

The implementation follows the principles in the primary
[DDGI production reference](https://jcgt.org/published/0010/02/01/) and
[SVGF reconstruction paper](https://research.nvidia.com/sites/default/files/pubs/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A/svgf_preprint.pdf).
The local filter is a small project-owned temporal/edge-aware denoiser, not a
complete SVGF implementation or a vendor runtime dependency.

## Verification

Windows GPU: AMD Radeon RX 9070 XT. Evidence paths below are repository-relative.

- Native application builds: `logs/build/lighting-stable-moments-build.log` and
  `logs/build/lighting-probe-capture-build.log`. The playable bundle was rebuilt
  in `logs/build/lighting-motion-repair-bundle-retry.log`, then updated with the
  final native executable and runtime shader payload. The initial parallel
  package build hit a NuGet restore file race; the serial retry passed.
- Twenty-one actual DDGI scheduler cases and SPIR-V/DXIL/Metal shader emission:
  `logs/client/validation/lighting-stability/ddgi-source-checks.json`.
  Metal emission is compile evidence only.
- DX12 and Vulkan player regressions: `logs/client/player-shadow-{dx12,vulkan}.log`.
  Six authored body regions, off-silhouette miss, first/third person, movement,
  hidden toggle, retained frame slots, ray sun/local and raster sun/local all pass
  without graphics validation warnings/errors.
- Actual eight-block lateral camera sweeps passed 900 frames each on DX12 and
  Vulkan at 960x540 Native AA. Both movement extremes were visually inspected.
  An additional DX12 1280x720 FSR Quality/third-person run passed; the inspected
  capture shows the player's full silhouette shadow on the ground. Results:
  `logs/client/lighting-motion-repair/final/lighting-lateral-*/result.json`.
- Matched denoiser on/off fixed-camera captures measured 27.4% lower mean temporal
  luminance deviation on the floor and 48.8% lower near a torch, with almost
  unchanged mean brightness. Denoiser median cost was 0.154ms at 960x540 in that
  fixture. Evidence: `logs/client/lighting-motion-repair/denoiser-comparison.json`.
  These measurements predate the final fine-cascade payload; they isolate the
  denoiser and are not a final-cascade GI error or FPS guarantee.
- Final enclosed digging runs: 1,200 frames and 48 captures each, DX12 one-block
  tunnel and Vulkan two-block tunnel, under `logs/client/lighting-final/`.
  Real server commands close the initial spawn skylight, remove the wall block
  and replace it. Opening and restored frames were inspected on both APIs.
  The original GI-only isolation found pronounced coherent wall pulsing; the
  final filter/classification changes reduce its mean pairwise luminance change
  by 98.0% on DX12 and 97.1% on Vulkan. Floor and left-wall measurements improve
  as well. The comparisons retain brightness and residual variation rather than
  claiming a zero-error or flicker-free renderer.
  Probe sidecars additionally verified 49,880 DX12 and 49,263 Vulkan consecutive
  valid-geometry pairs without an unrequested relocation or validity loss.
- The final runtime payload also passed the 900-frame DX12 1280x720 FSR Quality
  third-person lateral sweep in `logs/client/lighting-final/lighting-lateral-dx12-95794vof`.

The explicit diagnostic helpers use isolated worlds and real server-authoritative
block commands. They do not inject operating-system mouse or keyboard events.
No diagnostic flags or worlds were made ordinary startup defaults.
`logs/client/lighting-final/bundle-manifest.json` records the installed executable
and complete shader payload; all 97 first-party shader source files match the
playable bundle. Vendor shader/license payloads are recorded separately from
that first-party source comparison.

## Limits

Finite probe coverage, sparse Monte Carlo sampling and temporal convergence
remain approximations. The subsequent [cave repair](cave-lighting-generator-repair.md)
adds pending-fine handover, shared recursive histories and visibility-tested
sky outside the volume; this report's earlier capture results predate that change.
The probe debug overlay describes the coarse grid. World items do not cast the
new dynamic shadows, and player/items remain forward light receivers. Radius32,
sustained fast travel, other GPU vendors, Linux hardware and macOS runtime are
not established by these focused Windows captures.
