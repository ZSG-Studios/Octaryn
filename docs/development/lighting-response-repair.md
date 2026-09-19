# Lighting response and daylight repair — 2026-09-17

**Current direction (2026-09-18):** the user restored DDGI after withdrawing the
experimental replacement. See [DDGI restoration](ddgi-restoration.md). This report
retains the original DDGI measurements and convergence limitations; restoration
does not turn those historical limitations into successful qualification.

The reported issues are stale probe lighting after torch removal, slow probe
updates, dark daytime ambience, and the last session closing unexpectedly.

## Shutdown evidence

Windows events and the local server log identify a server exception at 04:51:12
EDT while replacing `runtime/block_results.json`. The client exits when its
bundled server stops. Receipt publication now retries transient mailbox I/O and
access errors while retaining the bounded receipt queue and save-before-publish
ordering. See [the closure report](block-receipt-closure-repair.md).
An isolated production-server run reproduced HRESULT `0x80070005`, persisted
the accepted edit while the mailbox was locked, continued ticking, then
published and retired exactly one receipt after unlock and acknowledgement.
It shut down with exit 0. Evidence:
`logs/server/receipt-contention-20260917/receipt-contention-iexww6re/result.json`.

## Direct torch lighting

The production mesh probe exercises predicted placement/removal, rejection,
authoritative revision coverage, coalesced acknowledgements, reset and eviction.
On Windows DX12, direct GPU lighting returned to zero after the last emitter was
removed. This did not reproduce a stale CPU emitter registry or direct-light
texture. The run reported zero graphics validation errors and 54 repeated
duplicate-resource-barrier warnings; it was not warning-clean.

Evidence: `logs/client/torch-source-lifecycle-dx12-result.json` and its native log.
These readbacks alone do not establish DDGI convergence or visual correctness.

## Captured stale-probe baseline

The isolated torch fixture captured cold, added and removed states at one camera
pose. With voxel GI radius 6, all 1,728 probe metadata records were unchanged from
renderer frame 1654 through 2554, about 1.985 observer seconds. Their last-update
frames remained 1650–1651. Removed captures had no local lights or local visibility
rays; ray-scene generation 165 reached 81 ready columns with no pending builds.
The image remained darker than its cold baseline. With voxel radius 0, the old
payload allocated no GI probes and returned closely to its cold appearance.

This is direct evidence of stalled updates and history-dependent appearance,
not a reproduction of every detail in the reported screenshot. These historical
captures use an even stride and cover one frame parity. The corrected runner
uses stride 31 and requires both parities. Probe metadata alone is not irradiance
readback; observer timing is not an exact GPU publication timestamp. Evidence:
`logs/client/lighting-response-before/torch-fine6-53i48lok/probe-response-analysis.json`
and `torch-fine0-bj2bnoj5/probe-response-analysis.json` in the same parent directory.

## Daylight and uncovered receivers

The original reference ambient formula was inspected at upstream commit
`3557cbfdc803ec034122bb55070b62b3b43b5588`, in
`references/old-architecture/source/shaders/{shader_common.glsl,composite.comp.glsl}`.
The active linear HDR ambient response now smoothly increases daylight radiance
from its existing night response to 2.5 times the previous full-day response.
The shared function supplies both the probe environment and raster ambient;
probe visibility and sky occlusion still apply. Saved lighting controls are
not rewritten.

The existing `DDGISkyVisibility.slang` implementation was disconnected from HDR:
`CompositeRT.slang` merely included the ordinary composite, and the HDR owner
always selected the ordinary pipeline. The ray composite now evaluates visible
sky for uncovered DDGI receivers and binds the existing scene and material atlas.
Both frame slots already retain the ray composite pipeline. Fully covered
receivers avoid these additional visibility rays.
The activated sky helper also needed a correction: an any-hit foliage result
previously admitted partial skylight without checking the opaque wall behind
it. A dedicated sky query now continues through transmissive faces and rejects
the direction on any opaque hit. Transmission multiplication is independent of
candidate order. Vertical receivers skip directions with zero contribution
before tracing. GPU coverage and composition timing remain part of qualification.

An ambient-only calibration on the original isolated native/DDGI payload passed
the production DX12 lighting qualifier. Actual before/after screenshots were
inspected: stone and shaded wood/stone detail remain visible, and the display sky
is unchanged. Matched tone-mapped luminance means increased from 0.34140 to
0.41099 on the open stone receiver and from 0.35342 to 0.39384 on the pillar.
Sky means were 0.59411 and 0.59414. This isolates the ambient adjustment; it does
not qualify the subsequent combined DDGI/HDR changes. Evidence:
`logs/client/lighting-response-daylight-calibration/comparison.json` and
`lighting-dx12-high-7n21ckav/frame.png`, compared with
`logs/client/lighting-response-before/lighting-dx12-high-lsue252k/frame.png`.

## Qualification status

The combined repaired DDGI/HDR client linked successfully and completed the
DX12 daylight fixture. Its actual screenshot was inspected. Explicit capture
metadata introduced booleans, vectors and floating-point energy; the existing
integer-only validator initially rejected those fields. The validator now checks
their types and finite ranges, and the completed run was reanalyzed successfully.
Evidence: `logs/client/lighting-response-final-daylight/lighting-dx12-high-edb2qx0y`.

Cold/add/remove runs completed for fine+coarse, coarse-only, fine-only and disabled
GI, using 64 captures with both frame parities. The uncapped fine+coarse run still
showed cold-state drift and a residual difference, so full convergence was **not**
accepted before the direction changed. Evidence roots are
`logs/client/lighting-response-after`, `lighting-response-after-fine-only`, and
`lighting-response-after-disabled`. These are measured results, not a complete
visual-parity pass. A separate ray-query regression stopped at the pre-existing
same-count BLAS-refit assertion before exercising its new sky-transmission cases;
`logs/client/lighting-sky-query-dx12.log` records that unresolved check.

The native client and server targets build successfully; evidence is
`logs/build/lighting-response-native.log`. Both HDR compute entry points compile
to SPIR-V, DXIL and Metal; `logs/build/hdr-response-shaders.log` records all six
successful compilations (Metal reports entry-point renaming). These are compiler
checks, not platform runtime qualification. Further DDGI convergence work was
paused during the subsequently withdrawn replacement. The then-running application and its
saves have not been replaced by this build. Windows results do not qualify
Linux Vulkan or macOS Metal execution.
