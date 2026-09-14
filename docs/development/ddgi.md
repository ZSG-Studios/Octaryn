# Dynamic diffuse irradiance fields

The client owns a bounded, camera-centered scrolling DDGI grid. `DDGISystem`
submits through the existing Slang RHI queue and shares `WorldRayQuery`, the
authoritative atlas/LabPBR materials, and `LocalLight` with the other lighting
passes. No NVIDIA SDK or backend-specific ray tracing dependency is introduced.

The irradiance/visibility representation follows
[Majercik et al., 2019](https://jcgt.org/published/0008/02/01/); the bounded
relocation and sleeping/inactive probe policies follow the principles in
[Majercik et al., 2021](https://jcgt.org/published/0010/02/01/).
These are independently implemented client shaders, not an RTXGI SDK integration.

## Pass and resource contract

1. Consume central column/acceleration-publication and local-light revisions.
2. Scroll logical integer grid cells through persistent toroidal physical slots.
3. Upload cell identities, scene-refresh requests and the bounded probe selection.
   Refreshes preserve irradiance and relocation while waking scheduled probes;
   scrolling a physical slot to a new world cell rejects its old history.
4. Trace fixed geometry rays and rotated spherical Fibonacci lighting rays
   against the existing TLAS. Fixed rays only classify and relocate probes and
   are excluded from irradiance and distance blending. Lighting ray hits
   use shared albedo, metallic, emission, two-sided vegetation and alpha testing.
   Hit radiance includes shadowed sun, one uniformly sampled local light with
   inverse-selection-probability compensation, and previously gathered indirect
   irradiance. Misses use the existing atmosphere without the direct sun disk.
5. Update octahedral irradiance and first/second distance moments. Adaptive
   hysteresis responds faster to abrupt changes. Relocate embedded probes by
   signed backface distances, capped to 0.45 of grid spacing; reject irradiance
   until the new location has been retraced. Classify stable probes as sleeping
   and trapped probes as inactive, with periodic retry and edit reawakening.
6. Composite diffuse irradiance divided by pi. Eight surrounding probes use
   trilinear, normal and cubed Chebyshev visibility weights. Only the volume edge
   fades to reduced GI; invalid or occluded probes inside it do not restore
   unoccluded ambient. A newly initialized volume fills its indirect light as
   probes are scheduled.

At the default settings, persistent resources consume 2,490,880 bytes:

| Resource | Layout |
| --- | --- |
| Cell controls | 2,048 x 32 bytes: integer cell, identity, refresh frame and padding |
| Probe states | 2,048 x 32 bytes: relocation/classification and history metadata |
| Irradiance | 2,048 x 6 x 6 x float4 |
| Distance moments | 2,048 x 8 x 8 x float2 |
| Trace results | 64 x 64 x 32 bytes |
| Selections | Two fence-owned 64 x uint buffers |

The grid survives render-resolution changes. Device lifetime remains owned by
`WorldRenderer`. Explicit barriers separate uploads, trace, update and sampling;
normal rendering performs no probe readback or host wait. Resources are allocated
at initialization, with selection uploads using the existing command encoder.

## Budgets and controls

`OCTARYN_CLIENT_DDGI=off` disables tracing. The `OCTARYN_DDGI_` environment prefix
accepts `COUNT_X`, `COUNT_Y`, `COUNT_Z`, `SPACING`, `HYSTERESIS`, `MAX_DISTANCE`,
`RAYS`, `BUDGET`, `IRRADIANCE_RESOLUTION`, and `VISIBILITY_RESOLUTION`.
Values are bounded at initialization. Defaults are 16 x 8 x 16 probes, spacing 4,
64 rays/probe, 64 scheduled updates, hysteresis 0.94, and maximum distance 64.

The default primary-ray budget is 4,096/frame, including 16 fixed geometry rays
per probe. Each of the remaining 48 rays may trace one sun and one selected
local-light visibility ray, giving a total upper bound of 10,240.
Sleeping/inactive GPU probes skip rays until their retry interval;
logged scheduled counts are upper budgets, not actual traced-ray measurements.
Modified initialized probes outrank new cells; distance and age break ties and
prevent stable probes from being permanently starved.

`ddgi_debug` provides irradiance, distance/variance, classification, relocation,
age, and colored logical cells with surface-intersecting probe-center markers.
DDGI trace/update timing uses the shared lighting timestamp profiler.

## Validation and limits

The added scheduler test runs the actual `DDGISchedule.cpp` implementation and
checks negative-coordinate flooring, toroidal uniqueness, retained scrolling
history, bounded updates, scene-refresh coalescing/wakeup, repeated streaming
publication history preservation, edit priority, age fairness and CPU layouts.
Trace and update shaders compile to SPIR-V and DXIL using pinned Slang 2026.17.1;
the update shader also emits Metal source. These checks do not establish GPU
image quality or runtime/platform support; the integrated lighting report owns
that evidence.

This first implementation has one scrolling volume, not multiresolution cascades.
It resolves low-frequency diffuse lighting; thin-wall leakage and relocation in
complex geometry need scene qualification. Small ray counts and uniformly sampled
local lights converge slowly with very many lights. Temporal filtering and a
bounded radiance clamp reduce outliers but introduce response latency/bias.
Probe state debug views are available; an exact active-probe telemetry readback
and full free-space probe-sphere overlay are not implemented. The fallback ambient
path remains explicit at/outside volume edges and on non-RT hardware.

## Voxel stability repair

Scene refreshes previously changed probe identity, discarding valid irradiance
and relocation throughout the camera volume during streaming. Refresh frames
now request bounded retracing without making those probes disappear from the
gather. Stable geometry rays prevent changing random ray rotations from driving
relocation. Probe sleeping considers every irradiance texel rather than one.

Visibility moments clamp distances to 1.5 times the probe-cell diagonal, so
distant sky misses cannot dominate local wall variance. The gather selects
neighbors from the normal-biased surface and applies trilinear weights after
visibility-weight suppression. Bright random samples keep temporal filtering;
scene changes and substantial darkening can respond faster. Shared material
emission and voxel-source-aware local visibility feed probe ray shading.

These changes were checked against the fixed-ray, distance-moment and gather
contracts in NVIDIA's [probe blending reference](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl)
and [irradiance gather](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/rtxgi-sdk/shaders/ddgi/Irradiance.hlsl).
They do not establish scene image quality or eliminate the single-volume,
finite-resolution and temporal-latency limits described above.
