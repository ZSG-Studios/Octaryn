# Dynamic diffuse irradiance fields

The client owns two bounded, camera-following scrolling DDGI grids. `DDGISystem`
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
   against the existing TLAS. Fixed rays classify and relocate probes and provide
   deterministic distance moments; they are excluded from irradiance blending. Lighting ray hits
   use shared albedo, metallic, emission, two-sided vegetation and alpha testing.
   Hit radiance includes shadowed sun, one uniformly sampled local light with
   inverse-selection-probability compensation, and previously gathered indirect
   irradiance from the same density-ordered hierarchy used by final shading.
   Both cascades trace before either updates its field. Misses use the existing
   atmosphere without the direct sun disk.
5. Update octahedral irradiance and first/second distance moments. History starts
   with a running average and uses four updates at at most 0.5 hysteresis after
   scene changes, then returns to the configured steady-state filter. Relocate embedded probes by
   signed backface distances, capped to 0.45 of grid spacing; reject irradiance
   until the new location has been retraced. Classify stable probes as sleeping
   and trapped probes as inactive, with periodic retry and edit reawakening.
6. Composite diffuse irradiance divided by pi. Eight surrounding probes per volume use
   trilinear, normal and cubed Chebyshev visibility weights. Only the volume edge
   fades to visibility-tested sky; the fine volume blends into coarse coverage.
   Pending fine probes leave coarse coverage available. Evaluated dark/embedded
   fine probes suppress coarse leaking. Uncovered pixels in active DDGI test sky
   visibility instead of restoring unoccluded ambient, including underground.
   Horizon samples and leaf/cutout transmission keep canopy undersides from going
   black; solid cave hits still contribute nothing.
   See [cave repair](cave-lighting-generator-repair.md) for GPU evidence and cost.

At the default settings, coarse resources consume 14,500,608 bytes:

| Resource | Layout |
| --- | --- |
| Cell controls | 12,288 x 32 bytes: integer cell, identity, refresh frame and padding |
| Probe states | 12,288 x 32 bytes: relocation/classification and history metadata |
| Irradiance | 12,288 x 6 x 6 x float4 |
| Distance moments | 12,288 x 8 x 8 x float2 |
| Trace results | 96 x 112 x 32 bytes |
| Selections | Two fence-owned 96 x uint buffers |

The fine cascade adds 1,728 probes in a 12 x 12 x 12 grid with one-block spacing,
64 scheduled updates and the same atlas resolutions/ray count. Its half-cell
anchor puts probes at voxel centers, including one-block-wide tunnels that the
eight-block grid can miss. Its resources consume 2,220,544 bytes, for a combined
16,721,152 bytes. Both allocations remain fixed during movement and resizing.

The grid survives render-resolution changes. Device lifetime remains owned by
`WorldRenderer`. Explicit barriers separate uploads, trace, update and sampling;
normal rendering performs no probe readback or host wait. Resources are allocated
at initialization, with selection uploads using the existing command encoder.

## Budgets and controls

`OCTARYN_CLIENT_DDGI=off` disables tracing. The `OCTARYN_DDGI_` environment prefix
accepts `COUNT_X`, `COUNT_Y`, `COUNT_Z`, `SPACING`, `HYSTERESIS`, `MAX_DISTANCE`,
`RAYS`, `BUDGET`, `IRRADIANCE_RESOLUTION`, and `VISIBILITY_RESOLUTION`.
Values are bounded at initialization. Defaults are 32 x 12 x 32 probes, spacing 8,
112 rays/probe, 96 scheduled updates, hysteresis 0.94, and maximum distance 96.
That keeps full-weight coarse coverage through about 3.5 chunks from the camera,
including tree canopies above eye height. The previous 16 x 8 x 16 spacing-4
volume faded by 16–24 blocks, so distant leaf undersides lost bounce and went
black under sun occlusion.

Probe slots stay on the regular grid. CPU occupancy marks solid interiors inactive
so they leave the 8-probe cage, matching RTXGI classification. A probe is classed
as open sky only when its voxel sits above the topmost occupied voxel in the same
(x,z) column (a memoized per-revision heightmap). Anything at or below the column
top is traced ("Needed"), so tunnel and cave air, foliage undersides and overhang
shadows can never be mistaken for open sky. The earlier local six-block
neighborhood scan turned cave-interior air into permanently untraced sky probes
that held neighbor-seeded daylight, which lit sealed pitch-black rooms and never
converged. Crossing the open-sky boundary in either direction snaps the probe with
a full retrace so stale sky light (or stale shade) is dropped immediately.
Open-sky cells stay in the cage and keep a lower trace priority: they store
environment irradiance the gather still needs. A newly scrolled or newly opened probe copies irradiance and distance from
already-stable probes along each axis before it is used, so the leading
edge is not black and a broken block does not flash sky. Donors must first see
the seeded probe through their own stored distance moments, so a sunlit probe
cannot donate through an intact wall; when no visible donor exists in a fully
fresh region, the nearest stable donors are used as before. Holding break only
places the probe that occupies that voxel; it is seeded from neighbors but
stays out of the 8-probe cage and is not traced until the voxel is actually
empty, so its speculative field cannot leak through the unbroken block.
Neighbors keep seeing the block until the acceleration structure updates. Pending
fine probes still fall through to coarse coverage. Edit invalidation is
local (about three probe spacings), not the full gather radius. Releasing break
without an edit drops the speculative hole. Light list changes wake only the
probes inside each changed light's influence bounds — both the previous and the
new publication, so moved or removed lights refresh exactly the region they
used to touch. Water reflection hits evaluate direct sun plus DDGI diffuse
bounce with the same coverage blend as the composite, so reflected terrain
receives indirect light instead of flat ambient.

Shading follows the RTXGI gather: wrap-shading with a 0.2 floor, Chebyshev
visibility that never fully rejects, and perceptual crush below 0.2. Surface
bias is a fixed 0.2-block normal offset only. A camera-ray view bias (RTXGI's
default scaled 4x) moved the 8-probe cage and Chebyshev test as the player
looked around, which crawled GI shadows across voxel faces. Variance and
distance floors keep the visibility test off the mean of the probe depth
distribution without depending on view.

The default primary-ray budget uses tiered ray counts: 64 fixed geometry rays per
probe plus a per-selection lighting tier — burst (fresh or dirty, all 112),
active (updated within 480 frames, 48) or background (16). The coarse volume
uses 96 probe updates per frame; the fine volume scales with its radius.
Sleeping probes skip tracing entirely for 120 frames before revalidation.
Modified initialized probes outrank new cells, with one quarter of the update
budget reserved for new cells so repeated streaming publications cannot starve
their initialization. Distance and age break ties. With a single-update budget,
new cells get one out of every four frame opportunities.

`ddgi_debug` provides irradiance, distance/variance, classification, relocation,
age, and colored logical cells with surface-intersecting probe-center markers.
This debug view shows the coarse volume. Debug views 21–24 additionally draw
world-space probe spheres for the coarse and fine volumes: shaded from each
probe's own octahedral irradiance map, or colored by state with a white flash
on the last-two-frames trace wavefront so scroll and edit update waves are
visible live. DDGI trace/update timing uses the shared
lighting timestamp profiler and includes both volumes. Frame logs report their
combined scheduled rays, updated probes, invalidations and allocation bytes.

## Validation and limits

The added scheduler test runs the actual `DDGISchedule.cpp` implementation and
checks negative-coordinate flooring, toroidal uniqueness, retained scrolling
history, bounded updates, scene-refresh coalescing/wakeup, repeated streaming
publication history preservation, edit priority, age fairness and CPU layouts.
The lighting-stability extension passes 21 checks, including continuous coverage
across positive/negative scrolling boundaries, bounded new-cell progress under
continuous publication, voxel-center fine probe invalidation, full fine coverage
1.5 blocks above the camera and a complete 1,728-probe post-edit refresh within
27 frames at the default fine budget. This proves scheduling, not GPU convergence.
Trace and update shaders compile to SPIR-V and DXIL using pinned Slang 2026.17.1;
the update shader also emits Metal source. These checks do not establish GPU
image quality or runtime/platform support; the integrated lighting report owns
that evidence.

The two cascades resolve low-frequency diffuse lighting; thin-wall leakage and relocation in
complex geometry need scene qualification. Small ray counts and uniformly sampled
local lights converge slowly with very many lights. Temporal filtering and a
bounded radiance clamp reduce outliers but introduce response latency/bias.
Probe state debug views are available; an exact active-probe telemetry readback
and full free-space probe-sphere overlay are not implemented. The fallback ambient
path remains explicit at/outside coarse volume edges and on non-RT hardware.
The fine volume has a camera-centered ten-block fade span, with full weight in
the central six blocks on each axis. It covers nearby floors and ceilings without
depending on an eight-block probe reaching the tunnel. It does not provide fine
probe density throughout distant caves. Each trace currently obtains recursive
bounce feedback from its own volume; distant multi-bounce energy outside the fine
volume remains a qualification limit. A newly exposed or relocated fine probe
is seeded from the nearest stable neighbors so the region does not go black
while it initializes; coarse irradiance still does not leak through an occluder.

## Voxel stability repair

Scene refreshes previously changed probe identity, discarding valid irradiance
and relocation throughout the camera volume during streaming. Refresh frames
now request bounded retracing without making those probes disappear from the
gather. Stable geometry rays prevent changing random ray rotations from driving
relocation. Probe sleeping considers every irradiance texel rather than one.

Visibility moments clamp distances to 1.5 times the probe-cell diagonal, so
distant sky misses cannot dominate local wall variance. The gather selects
neighbors from the normal-biased surface and applies trilinear weights after
visibility-weight suppression. Bright and dark random samples keep temporal
filtering; explicit scene changes respond faster. Shared material
emission and voxel-source-aware local visibility feed probe ray shading.

These changes were checked against the fixed-ray, distance-moment and gather
contracts in NVIDIA's [probe blending reference](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl)
and [irradiance gather](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/rtxgi-sdk/shaders/ddgi/Irradiance.hlsl).
They do not establish scene image quality or eliminate the finite-resolution and
temporal-latency limits described above.

## Digging and scrolling stability

Probe refreshes now consume acceleration-ready publications and removals. Raster
column upload events no longer spend the one-shot edit refresh against the old
BLAS while its replacement is still building. Cell identity and relocation remain
intact. A four-update reactive filter keeps responding after the first retrace;
previously a single 0.8-history update immediately returned to 0.94, which at the
32-frame coarse scheduling cycle gave approximately 359 additional frames of
half-life for the remaining stale irradiance. The reactive window reduces a
constant old contribution to 6.25% after four updates, before recursive-light and
sampling effects. This is a filter bound, not a measured visual convergence time.

Continuous camera coordinates determine coverage inside one guarded grid cell;
integer toroidal scrolling no longer jumps the fade by the probe spacing.
Initialized steady-state probes cap a bright sample's luminance increase before
history blending to the larger of 0.025 and half the existing irradiance luminance.
At the default history weight this limits a positive update to 3% of existing
luminance above the 0.05 floor. Chromatic ratios are retained. Ordinary dark samples
keep the configured history rather than accelerating random misses. Scene changes
and history warm-up bypass the bright limit so newly uncovered
light can respond. This is probe filtering. Current direct local lighting uses
deterministic tile evaluation with no local-light temporal filter.

The temporal filtering, brightness-change suppression, multi-resolution volume
blending and relocation principles were checked against NVIDIA's primary
[production DDGI paper](https://jcgt.org/published/0010/02/01/),
[probe blending source](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl)
and [volume integration guidance](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/Integration.md).
The implementation remains Octaryn-owned Slang/RHI code with no RTXGI dependency.

### Historical narrow-tunnel runtime isolation

These captures predate the deterministic direct-light replacement and its removal
of the local-light filter. They qualify the recorded probe-repair payload only.

The initial integrated one-block tunnel run exposed coherent GI fluctuations on
a fixed wall even after the local-light denoiser. Disabling DDGI while retaining
local lighting reduced its last-16-frame-pair mean absolute change from 0.016719
to 0.000020 in final tone-mapped luminance. A first mature-radiance cap reduced the
GI-enabled value to 0.012476, showing that radiance outliers were not the entire
problem. These runs are stored under `logs/client/lighting-repair-final` and
`logs/client/lighting-repair-filtered`; they are failure/isolation evidence, not
an assertion of stable GI.

Distance moments now use 64 deterministic spherical Fibonacci geometry rays with
an exponent of 16, while irradiance retains 48 rotating lighting rays. This is an
intentional Octaryn extension to the referenced SDK's rotating distance sampling:
unchanged geometry produces the same distance quadrature instead of changing
Chebyshev weights whenever lighting rays rotate. Actual geometry edits retrace
these rays and retain the explicit reactive window. The local-distance clamp and
occlusion weighting remain. Finite angular quadrature can introduce visibility
bias; tunnel wall leakage and temporal behavior require fresh GPU qualification.

### Cadence and validated geometry

The scheduler can retrace nearby probes every frame while distant probes update
once per volume sweep. Applying the same 0.94 history weight to both made nearby
lighting much noisier. Mature history now uses
`pow(hysteresis, min(elapsedFrames / nominalSweepFrames, 1))`, with the existing
running-average warmup and explicit four-update edit response. The history count
can reach 65,535, so it does not prematurely limit the mature filter. Sparse
probes retain the configured response rather than becoming even slower.

A validated probe retains its geometry classification and relocation until a
scene refresh or a new scrolling identity requests a recheck. Explicit frame
captures found a probe switching between valid and inactive at an identical
position in an unchanged scene. Locking accepted geometry prevents that lighting
dropout while still revalidating actual edits and acceleration publications.

Final one-block DX12 and two-block Vulkan tunnel runs each passed 1,200 frames
and 48 captures with authoritative opening/replacement. After replacement, the
problem wall's mean pairwise display-luminance change fell from 0.016719 to
0.000333 (DX12), and from 0.013831 to 0.000397 (Vulkan): approximately 98% and 97%
reductions. These are fixed-region, final-image measurements including all
lighting and convergence, not an unbiased GI error metric. Other measured
surfaces improved too. See `logs/client/lighting-final/*stability-comparison.json`
and [the integrated report](lighting-motion-repair.md) for full scope.
