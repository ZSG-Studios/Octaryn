# DDGI elapsed-time response repair

## Source findings

The starting September 17 source differed from the older two-cascade reports:
it allocated only one voxel-centered volume, enabled only when voxel radius was
nonzero. A saved voxel radius zero/coarse radius 128 silently disabled all GI.
The repair restores the independent coarse/fine ownership and dispatch ordering
from repository commit `ebe0d15`, without writing the user's saved settings.

Actual stale-cache causes when probes are enabled:

- Every clean probe waited 120 rendered frames, even if irradiance was still
  changing. Clean open-sky probes never refreshed again. GPU-side sleeping could
  additionally discard explicit CPU selections.
- History response depended on frame count and selected volume fraction. Even
  the later 10% minimum update took many multi-second sweeps to lose old energy.
- Removal only snapped half of the old light's reach. The scheduler cleared the
  hard-removal marker before uploading controls, so recursive tracing could read
  the irradiance that the marker was intended to reject.
- An edit between frames could receive the same timestamp as the previous trace.
- Neighbor seeding labelled borrowed irradiance as a completed trace and could
  bypass the donor visibility test, borrowing through a known wall.

The existing fixed geometry quadrature, rotated lighting rays, octahedral fields,
shared world ray queries and RHI submission remain the implementation. Reference
inspection included the reports `lighting-architecture`, `lighting-motion-repair`,
`cave-lighting-generator-repair`, `voxel-lighting-vegetation-repair`, `ddgi`, and
upstream RTXGI `rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl` (history reduction on
lighting changes, independent fixed geometry rays and bounded visibility moments).
No vendor implementation was copied into the renderer.

## Changes

- Monotonic CPU elapsed time controls refresh eligibility: 0.25 seconds for
  ordinary probes and 0.5 seconds for open-sky probes. These are eligibility
  intervals, not guarantees that an overloaded volume completes within them.
- Steady work uses the configured budget per 1/60 second, with fractional credit
  and the existing allocated dispatch capacity as the hard limit. Idle time is
  not banked. Edit/startup burst capacity is spent on pending dirty work.
- Per-selection elapsed times are uploaded in two fence-owned float buffers.
  The update shader uses `pow(hysteresis, elapsedSeconds * 60)`; the default 0.94
  corresponds to about 0.269 seconds of history decay time. Recursive multi-bounce
  convergence still depends on tracing, visibility, materials and volume coverage.
- Removed lights hard-refresh their full prior influence bounds. Markers survive
  through trace/update, prevent stale recursive donation, and retire next frame.
  Gentle overlapping wakes cannot downgrade a hard removal. Geometry publication
  and scrolling preserve their existing local invalidation ownership.
- Seed donation requires visibility and a completed previous-frame trace. Seeds
  cannot donate to other seeds in the same dispatch, and their first real trace
  replaces provisional history. Known solid/removal-only cages retain dark
  coverage instead of pretending no occlusion was evaluated.
- `DDGIStats::pending_probes` and `oldest_update_seconds` expose pre-selection
  backlog/age for explicit qualification without adding ordinary log spam.
- Coarse and fine volumes independently allocate, prepare, seed and trace. Both
  traces sample the same hierarchy before either volume updates. Coarse-only,
  fine-only, both-enabled and both-disabled settings are distinct states. Changing
  either radius flushes and reconfigures resources through the existing owner API.
  Original environment configuration is retained across disable/re-enable.
- The coarse field restores per-axis coverage fading; the newer minimum-axis
  sphere would clip a 32x12x32 field to its 12-cell height in every direction.
  Fine voxel-center alignment and spherical local fade remain intact. Occupancy
  now tests the actual coarse probe position, rather than a half-spacing offset.

Voxel radius 6 gives 1,728 probes, a 48-probe budget per 1/60 second,
1,024 maximum probes per dispatch, 64 fixed rays plus 112/48/16 lighting rays
for reset/ordinary/open-sky tiers. A completely eligible 1,728-probe steady sweep
therefore needs approximately 0.6 seconds at the default budget; solid cells
do not consume tracing work. Larger voxel radii remain bounded and can take longer.
Coarse radius 128 independently gives 32x12x32 probes (12,288), spacing 8,
budget 128 per 1/60 second and maximum dispatch 1,024. A completely eligible
coarse steady sweep takes approximately 1.6 seconds. Coarse radius is the nominal
horizontal grid half-span, as in the restored implementation; interpolation has
a guard collar and a two-cell fade. Explicit captures report actual potential
coverage bounds as well as requested radius, grid counts and spacing. The shallow
vertical reach is independent of the requested horizontal radius. Above radius
256, spacing grows to retain requested grid span within the 64x12x64 cell cap;
this is sparse probe sampling, not voxel geometry LOD. Fine remains independently
disabled when the saved voxel radius is zero.

## Verification and remaining integration

- Actual C++ scheduler/configuration: 35 regression cases, including documented
  coarse-128 dimensions, bounded larger radii, independent fine density, 30/60/144 Hz refresh and
  work-rate checks, hard-marker lifetime, between-frame edits, sky refresh,
  toroidal preservation, edit priority and bounded streaming initialization.
  Log: `logs/build/ddgi-response-scheduler.log`.
- Production trace, update and seed shaders compile to SPIR-V, DXIL and Metal
  source (nine cases). Metal reports its normal `main` to `main_0` rename warning.
  Log: `logs/build/ddgi-response-shaders.log`. Emitted Metal is not runtime proof.
- Focused native DDGI owner objects compile in the release Windows tree.
  Debug/capture and lighting-settings owners compile as well.
  Log: `logs/build/ddgi-response-native.log`. Application linking and canonical
  bundle publication are coordinated by the main task after these changes.

GPU qualification must compare a sealed night/cave fixture with no torch, torch
present, and removed torch at matched wall-clock intervals (including both frame
slots). Record local-direct output separately from DDGI irradiance and final HDR;
wait for the edited AS publication so retained old emissive geometry is not
mistaken for history. Inspect actual screenshots, probe ages/backlog and GPU pass
costs at stationary and scrolling cameras. Repeat with voxel radius 6/coarse 128,
the user's voxel zero/coarse 128 configuration, and coarse zero/fine 6. Compare
receiver energy against the original no-torch baseline, not merely a dark-looking
image. The main HDR owner reconnects visibility-tested sky for uncovered pixels.

Capture JSON now separates `coarse_valid_probes` and `fine_valid_probes`, plus
per-volume `*_irradiance_sum`, `*_irradiance_mean`, `*_irradiance_max` and
`*_irradiance_texels`. These are linear luminance measurements from actual GPU
irradiance texels of traced, current-version, non-embedded/non-solid probes;
pending removed-source histories and provisional seeds are excluded. Readbacks
occur only in explicit captures after the frame fence. Probe-state sidecars keep
the coarse `probes` array and add `fine_probes`.

These source/CPU/compile checks do not establish GPU convergence, visual quality,
performance improvement, or any additional hardware/platform qualification.
