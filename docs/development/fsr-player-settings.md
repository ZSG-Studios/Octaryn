# FSR player settings

Menu: Escape → World & display → FSR settings.

This page exposes player-facing image-quality controls for the pinned FSR
2.2.1 integration. Off remains the default. This is not an FSR version upgrade.

| Setting | Behavior |
| --- | --- |
| Quality preset | Off, Native AA, Quality, Balanced, Performance, Ultra Performance, Custom |
| Sharpening | Toggles FSR's RCAS pass |
| Sharpness | 0–100%, mapped to the SDK's 0–1 sharpness input |
| Custom render scale | One-third to full output resolution per dimension |
| Dynamic resolution | Uses completed GPU timestamp samples to adjust active render resolution |
| Target frame rate | 30–240 FPS; a GPU budget, not a frame limiter |
| Minimum / maximum scale | Bounds for dynamic resolution; minimum cannot exceed maximum |
| Current resolution | Actual internal and output dimensions from the renderer |
| Reset FSR | Stages defaults without applying them immediately |
| Apply & return | Applies and persists the chosen settings |

Unavailable controls are disabled. Native AA always renders at 100%, regardless
of remembered dynamic-resolution preferences. Back returns to display settings
with edits still staged. The existing display menu applies when closed.

Dynamic resolution retains maximum-sized textures and the FSR context/history.
It adjusts active viewport and dispatch dimensions using smoothed GPU timing,
a deadband, bounded increments and strict scale limits. CPU bottlenecks and
fixed-cost work can prevent the target frame rate from being reached.

Reactive masks, jitter, exposure conventions, motion vectors and mip bias remain
renderer responsibilities. FSR 2 does not provide frame generation, so there is
no nonfunctional frame-generation switch or unrelated latency-control toggle.

## Reference basis

- [AMD FSR 2 overview](https://gpuopen.com/fidelityfx-superresolution-2/)
  recommends exposing sharpening strength to players.
- [AMD's pinned FSR 2 source and integration documentation](https://github.com/GPUOpen-Effects/FidelityFX-FSR2)
  defines the quality ratios, sharpening dispatch and dynamic-resolution support.
- [AMD Super Resolution sample settings](https://gpuopen.com/manuals/fidelityfx_sdk2/samples/super-resolution/)
  documents preset/custom scale and the 0–1 sharpening range. This newer SDK
  sample is a control-design reference, not the shipped algorithm version.

## Verification

CPU settings validation passed 50 checks covering all seven modes, legacy
defaults, invalid input and exact persistence. The dynamic-resolution controller
passed 13 checks covering bounds, GPU samples, Native/Off exclusion and history.
The packaged FSR menu passed 51 interaction checks, alongside 679 existing UI
and 1723 inventory checks. Captures at 1280x720 and 640x480 were inspected under
`logs/client/validation/fsr-settings-v3`; the small window uses a scrollable menu.

`tools/validation/validate_fsr_player_settings.py` passed four 600-frame cases
under `logs/client/validation/fsr-settings-qualified`. Captured dispatch metadata
confirmed Custom 72.5% (928x522), sharpening disabled/enabled at 73%, GPU-driven
scale adjustment to the configured 85% ceiling on both DX12 and Vulkan, and
Native AA at 1280x720 despite a remembered dynamic-resolution preference.
There were no UI or graphics validation warnings/errors in the final cases.

The existing nine-phase temporal mode-switch/resize/camera-cut regression also
passed DX12 and Vulkan with two frames in flight; evidence is under
`logs/client/validation/fsr-settings-regression`. The modified HDR shader emits
Metal 3.1 source; native Metal execution remains unverified on this Windows host.

Validation caught and fixed range synchronization recursion, small-window menu
overflow, a controller boundary deadband, Vulkan clear-to-copy ordering, and
missing readback usage on temporal/depth textures. Earlier failed logs are
retained separately from qualified evidence.

FSR settings menu — local evidence: `logs/client/validation/fsr-settings-v3/menu-1280/frame.bmp` (not distributed in source).
