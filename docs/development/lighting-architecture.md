# Integrated voxel lighting — 2026-09-14

The active native client now combines hardware-ray-traced sun shadows, scrolling
DDGI diffuse illumination, ReSTIR local direct lighting, and raster shadow
fallbacks through standalone Slang RHI. It extends the existing exact procedural
voxel RT baseline; it does not introduce a second mesh, material, BVH, backend,
or server lighting system. Real deferred terrain feeds the effects, followed by
the existing forward presentation and FSR.

Windows DX12/Vulkan qualification on the AMD Radeon RX 9070 XT is recorded below,
including the final selected-local-light raster fallback on both APIs. This
is a working foundation with the explicit limits in section 13, not complete
implementation of every optional future feature.

## 1. Architecture

~~~mermaid
flowchart TD
    World[Authoritative voxel changes and streaming] --> Mesh[Existing GPU greedy surface quads]
    Mesh --> Scene[Bounded column BLAS builds or refits]
    Scene --> TLAS[Retained TLAS snapshots]
    World --> Changes[Central scene-change journal]
    Mesh --> GBuffer[Depth and terrain GBuffer]
    TLAS --> DDGI[Probe rays and irradiance fields]
    TLAS --> Sun[Stochastic RT sun and temporal filter]
    TLAS --> Local[ReSTIR selected-light visibility]
    Lights[Validated point, spot and rectangle lights] --> Local
    Lights --> DDGI
    Changes --> DDGI
    Changes --> Sun
    Changes --> Local
    Mesh --> Fallback[Sun clipmaps and selected local shadow maps]
    GBuffer --> Compose[HDR emission + direct sun + direct local + DDGI]
    DDGI --> Compose
    Sun --> Compose
    Local --> Compose
    Fallback --> Compose
    Compose --> Present[Existing forward layers, FSR, tone mapping and UI]
~~~

LightingGraph describes bounded passes by resource dependencies. All passes use
the existing ordered command queue; no unmeasured async-compute overlap or new
host synchronization is introduced. Probe/reservoir readbacks occur only in
explicit qualification captures after their frame fence.

## 2. Added files

Paths below are relative to the repository root. This is the lighting manifest;
unrelated pending UI, cloud, release and reference-layout changes are not
attributed to this implementation.

Under octaryn-client/Source/Rendering/RenderBackend/:

- Coordination: RendererCapabilities.h/.cpp, SceneChanges.h, LightingGraph.h,
  LightingProfile.h, LightingQuality.h, LightingSystem.h/.cpp, LightingCapture.cpp.
- AS owner split/diagnostics: WorldRayTracingState.h, WorldRayBuild.cpp,
  WorldRaySnapshot.cpp, RayTracingTiming.h, WorldRayDebug.h/.cpp.
- DDGI: DDGISystem.h/.cpp, DDGISchedule.cpp.
- Local lighting: LocalLight.h, ReSTIRDISystem.h/.cpp, ClusteredLocalLights.cpp,
  LocalShadowSystem.h/.cpp.
- Directional shadows: RTShadowSystem.h/.cpp, ShadowFallbackSystem.h/.cpp.

Under octaryn-client/Shaders/:

- DDGI/DDGITypes.slang, DDGI/DDGISample.slang, DDGI/DDGITrace.slang,
  DDGI/DDGIUpdate.slang, DDGI/DDGIEnvironment.slang.
- Lighting/Surface.slang, Lighting/LocalLight.slang, Lighting/LocalLighting.slang,
  Lighting/Reservoir.slang, Lighting/ReSTIRInitial.slang,
  Lighting/ReSTIRTemporal.slang, Lighting/ReSTIRSpatial.slang,
  Lighting/ReSTIRResolve.slang, Lighting/ReSTIRVisibility.slang,
  Lighting/ReSTIRFallback.slang, Lighting/ClusteredLocalLights.slang,
  Lighting/ClusteredLocalResolve.slang.
- Shadows/Temporal.slang, Shadows/ClipmapRaster.slang, Shadows/ClipmapResolve.slang,
  Shadows/LocalRaster.slang, Shadows/LocalSample.slang, RayTracing/Debug.slang,
  RayTracing/BoundsDebug.slang.

Validation/documentation: tools/validation/ddgi_schedule_test.cpp,
tools/validation/restir_math.cpp, tools/validation/validate_shadow_plane.py,
tools/validation/validate_lighting_architecture.py, this report,
[ddgi.md](ddgi.md), and [restir-di.md](restir-di.md).

## 3. Modified owners and removed overlap

- Renderer integration in RenderBackend: WorldRenderer.cpp/.h,
  WorldRendererInternal.h, WorldRendererDevice.cpp, WorldDraw.cpp, WorldBatch.cpp,
  WorldCapture.cpp.
- Existing RT owners: WorldRayTracing.h/.cpp, WorldRayLighting.h/.cpp,
  Shaders/RayTracing/WorldRayQuery.slang and Shaders/RayTracing/Shadow.slang.
  Their original exact-quad RT baseline, bounds shader and ray-aware water
  renderer were reused; those preexisting features are not claimed as new here.
- HDR: Source/Rendering/Hdr/WorldHdr.h/.cpp and Shaders/Hdr/Composite.slang.
- Shared atlas: Source/Rendering/Atlas/WorldAtlas.h/.cpp and its call sites use
  named shader bindings. This removes the positional layout assumption exposed
  when DDGI introduced resources before atlas declarations.
- Coordinated portability fixes from the release task: Atlas/AtlasAnimation.cpp
  accepts CRLF metadata on Linux; WorldRendererDevice.cpp selects a supported
  linear RGBA/BGRA presentation format suitable for compute storage. The release
  task tested those fixes independently; this integrated lighting build still
  requires separate Linux runtime qualification.
- Native/build validation: cmake/Owners/ClientTargets/ClientNativeLibraryTargets.cmake,
  cmake/Owners/ToolTargets/ToolWorldMeshProbe.cmake, the existing
  tools/Source/ClientWorldMeshProbe/RayTracing.cpp oracle,
  tools/validation/validate_client_shader_bundle.py and the existing capture path.

Shaders/Lighting/DDGIReady.slang was removed; actual probe resources replace the
unused summary stub. The former standalone sun-shadow dispatch is replaced by
the integrated shadow owner. Original hemisphere ambient remains only as the
explicit reduced GI mode when valid DDGI coverage is absent. Reference material
and external backup workspaces were not changed by this lighting work.

## 4. Important APIs

| Owner/API | Responsibility |
| --- | --- |
| renderer_capabilities / RendererCapabilities | Exact RHI feature inventory and usable inline-lighting gate |
| SceneChanges::notify_column / for_each_since | Bounded revision journal with overflow detection |
| world_ray_prepare / world_ray_bind / world_ray_set_build_budget | Shared AS preparation, binding and build budgets |
| DDGISystem / world_ddgi_initialize, update, bind | Persistent probes, tracing and surface sampling |
| ReSTIRDISystem / world_restir_prepare_lights, update, bind | Light input, persistent reservoirs and local direct output |
| open_world_renderer_set_lights | Validated point/spot/rectangle input, bounded to 65,536 lights |
| RTShadowSystem / ShadowFallbackSystem / LocalShadowSystem | Sun RT/history, directional clipmaps, selected local raster shadows |
| open_world_renderer_set_lighting_options | Validated quality, angular radius, history weight, resolution and debug options |
| LightingGraph / LightingProfile / capture_lighting | Dependencies, per-pass GPU timestamps and diagnostic counters |

## 5. Active render-pass ordering

1. Reuse the completed frame slot; resolve timestamps and apply pending mesh/atlas
   updates. Poll bounded AS jobs and publish/refit/build the TLAS.
2. Render the existing sky and opaque/sprite depth/GBuffer.
3. DDGI consumes scene/light revisions, scrolls/schedules probes, uploads light
   changes, traces rays and updates irradiance, moments and probe state.
4. Local lighting generates candidates, performs temporal then spatial reuse,
   and resolves selected visibility. Low/medium also generate tile lists and
   refresh the selected local-light shadow cache.
5. Sun lighting runs RT visibility plus temporal filtering on high/ultra, or
   clipmap rendering and PCF resolve on low/medium/non-RT devices.
6. HDR composition adds emission, diffuse sun, local direct light and DDGI.
   Optional RT debug overlays follow composition.
7. Existing forward water/lava/glass, player/items and clouds, temporal input/FSR,
   tone mapping, selection/UI and presentation retain their existing owners.

Resource transitions and barriers separate reads from writes. Device teardown
drains submitted work. Resolution-dependent histories resize/invalidate; the
world-space DDGI grid survives render-scale changes.

## 6. GPU resources and default budgets

| Resource | Storage/default |
| --- | --- |
| Terrain GBuffer | Existing RGBA16F color, RGBA32F camera-relative position, RGBA8 packed voxel/material, D32 depth |
| AS scene | Mesh/fluids descriptor handles, face counts and column identity; one procedural BLAS/column and retained TLAS snapshots |
| DDGI | 16x8x16 probes; 6x6 float4 irradiance, 8x8 float2 moments, state/controls and two selections: 2,458,112 bytes |
| DDGI updates | 64 probes x 64 primary rays = 4,096 scheduled rays/frame; at most one sun and one local visibility ray/hit, total upper bound 12,288 |
| Local lights | 80 bytes/light; validated CPU input and retained GPU buffer |
| ReSTIR | Four 32-byte/pixel reservoirs, 32-byte/pixel surface history, 8-byte/pixel output: 168 bytes/pixel, about 590.6 MiB at 1440p before tiles/lights |
| Local tiles | 16x16 tiles, bounded uint lists, default 64 lights/tile; overflow retains full-support sampling |
| RT sun history | Two sets of raw R32F, visibility/age RG32F, position RGBA32F and voxel RGBA8: 64 bytes/pixel, about 225 MiB at 1440p |
| Sun fallback | Three D32 clipmaps, default 1024 square: 12 MiB; low startup uses 512 square |
| Local fallback | Six cached D32 faces for one selected light, default 256 square: 1.5 MiB |

Reservoir storage is allocated when lights exist; empty startup retains a
cleared output texture and dummy light binding. Shadow resources allocate when
their path first runs. These numbers exclude the rest of the renderer, AS
overhead and driver memory; they are not whole-game VRAM totals.

## 7. Acceleration structures and scene changes

Existing GPU greedy quads produce procedural AABBs, then inline Slang queries
test each candidate's actual two triangles. Alpha cutoff and material flags
come from the raster atlas/catalog. Shared geometry reconstruction covers
vegetation and retained fluid heights. Water/glass transmit current visibility
rays; opaque terrain and lava occlude them. Hits expose instance, primitive,
material, position, normal, UV, packed voxel and backface information.

BLAS ownership follows streamed 32x32 columns, not a duplicate mesh per effect.
All resident columns participate, including off-camera casters. Four pending
jobs are allowed; normal budgets are two builds/frame and 262,144 faces/frame.
Same-count edits can refit into a new retained BLAS; bounded repeated refits
eventually rebuild. Topology changes rebuild. Equal-instance-count changes can
update the TLAS. Camera motion alone does not rebuild unchanged AS geometry.

Snapshots, update sources, geometry and scratch buffers remain alive until
their fences allow release. Empty scenes use the existing masked real instance
required by the pinned RHI. SceneChanges retains 256 Added/Modified/Removed/
AccelerationReady events; cursor overflow forces conservative invalidation.

## 8. DDGI

[Detailed DDGI contract](ddgi.md). Logical integer cells scroll through a
toroidal volume; wrapped cells get new revisions to reject old world history.
Rotated Fibonacci rays gather sky, emission, shadowed sun, a uniformly sampled
local light with inverse-proposal compensation, and prior indirect lighting at
the hit. Direct illumination of a hit becomes a bounce at the visible receiver;
the composition does not add that receiver's direct lighting twice.

Octahedral irradiance and distance moments use hysteresis and eight-probe
normal/trilinear/cubed-Chebyshev interpolation. Signed solid-backface distances
drive relocation capped at 0.45 spacing; two-sided vegetation does not imply
an embedded probe. Invalid/relocated histories are rejected. Sleeping/inactive
probes skip expensive rays until retry, while geometry/light changes reawaken
them. Edits, newly exposed cells, distance and age drive bounded scheduling.
Coverage blends to reduced ambient over two probe cells with a smoothstep curve,
retaining a fully weighted interior in the default eight-cell vertical grid.

## 9. ReSTIR DI and local lights

[Detailed reservoir contract](restir-di.md). Candidates use correct
target/proposal weights with stored light/sample, sum, target and represented
count. Temporal/spatial reuse re-evaluates destination targets and reduces
weight and count together when limiting history. A positive target floor
preserves common sampling support. Temporal-only reservoirs feed the next
temporal history, avoiding recursive spatial feedback.

Reprojection includes previous camera basis/projection and raster jitter.
Position, voxel identity and normal reject disocclusion; camera cuts,
scene/light revisions and extent changes invalidate history. High-quality
visibility traces only the selected contributing light. Point, spot and
rectangle incident radiance shares one representation with DDGI. High-quality
candidates are currently uniform, not a light hierarchy or RTXDI dependency.

## 10. Sun RT shadows

The sun uses at most one stochastic disk sample per eligible visible pixel.
Interleaved-gradient noise and temporal rotation sample configurable angular
radius; zero gives hard shadows. Shared queries apply origin/normal bias,
alpha-tested geometry and material transmission. A separate history pass
reprojects world positions, validates packed voxel and scene/light/camera
state, and accumulates visibility with bounded age/hysteresis. Visibility
affects direct sunlight; DDGI is the separate indirect term.

## 11. Raster fallback and settings

Directional fallback uses three camera-centered light-space clipmaps with half
spans 64, 256 and 1024 world units. Texel-snapped centers stabilize translation.
Off-camera casters are included. PCF corrects receiver-plane depth per tap;
sprite cutoff/clamping matches shared material sampling, and lava uses actual
fluid geometry rather than full-height proxies.

Local fallback adds one selected important point/spot light's six-face depth
cache to GPU tile lighting. Selection weights intensity/luminance by camera
distance. Maps refresh on selection, range, light or scene changes; unchanged
maps are reused. Default shadow range is 64 units. Other lights, rectangles and
distances outside the selected map remain unshadowed. Final low-tier runs on
both APIs exercise this cache; earlier high-tier runs do not establish it.

Low/medium choose raster sun/local shadows and tiled local lighting.
High/ultra choose RT sun and selected visibility when supported. Low reduces
initial candidates and disables spatial reuse; ultra increases candidates and
spatial neighbors. DDGI is independently configurable and requires usable RT;
OCTARYN_CLIENT_DDGI=off selects reduced GI. Presets do not imply DDGI cascades.
Public options and OCTARYN_DDGI_* controls separate budgets from preset choices.

## 12. Capability detection, debug views and telemetry

Capabilities query the pinned RHI for AS, inline queries, ray pipelines,
bindless resources, waves, indirect commands, timestamps, FP16 and atomics.
Actual inline lighting requires AS + RayQuery + bindless; ray-pipeline capability
alone is reported but not substituted. Unsupported devices use raster fallback.
OCTARYN_CLIENT_RAY_TRACING=required fails initialization when requirements are
missing; auto permits fallback and off disables RT. Storage binding limits not
exposed by RHI are reported as unreported rather than fabricated.

OCTARYN_CLIENT_LIGHTING_DEBUG / public debug_view selects:

| Mode | View |
| --- | --- |
| 0 | Composed lighting |
| 1 | Sun visibility, including raster fallback |
| 2–7 | DDGI irradiance; distance/variance; state; relocation; age; logical cells and surface center markers |
| 8 | Local-light contribution |
| 9 | TLAS instance colors, highlighting missing ready-RT coverage |
| 10 | BLAS/column bounds line overlay |
| 11 | RT hit distance |
| 12 | Sun history age and visibility |
| 13–20 | Selected light ID; age; M; temporal acceptance; spatial reuse; visibility; weight sum; light count |

LightingProfile records AS, DDGI trace/update, ReSTIR initial/temporal/spatial,
local visibility, sun trace/filter and composition GPU timestamps. AS owners
also report build/refit/update counts, GPU times, pending/ready columns, retained
meshes and temporary memory. Normal logs distinguish DDGI scheduled ray budgets
from actual counts. Explicit captures read actual local visibility/reservoir
counters and active/valid/sleeping/inactive probe counts after their frame fence.

## 13. Validation and known limits

All GPU runs below used Windows and the RX 9070 XT. Result JSONs retain timing
and capture paths. The harness records screenshot inspection as a separate
requirement; an automated pass alone does not establish image quality.

| Evidence | Confirmed result |
| --- | --- |
| [DX12 high at 1440p](../../logs/client/validation/lighting/lighting-dx12-high-ji8u6dz0/result.json) | 600 measured frames, 2560x1440 native AA, 81 columns, 320,571 quads, exit 0 |
| [DX12 captured counters](../../logs/client/validation/lighting/lighting-dx12-high-ji8u6dz0/frame.bmp.lighting.json) | 2,118,882 selected visibility rays; 2,121,351 reservoirs; 2,101,753 temporal and 2,118,093 spatial acceptances; 2,043 valid of 2,048 probes |
| [Vulkan high and resize](../../logs/client/validation/lighting/lighting-vulkan-high-wt89vfzt/result.json) | 180 measured frames, base 960x540; nine production resize/upscaler phases, six modes, 173 successful phase frames; no reported validation warnings/errors |
| [Vulkan captured counters](../../logs/client/validation/lighting/lighting-vulkan-high-wt89vfzt/frame.bmp.lighting.json) | 236,399 selected visibility rays; 236,967 reservoirs; 2,045 valid of 2,048 probes |
| [DX12 AS oracle](../../logs/build/lighting-as-dx12.log), [Vulkan AS oracle](../../logs/build/lighting-as-vulkan.log) | Exact/signed/offscreen/range queries; zero camera rebuilds; two retained frames; edits, eviction, reload, edited air; refit, topology rebuild, TLAS update, notifications, journal overflow and retired-mesh release pass |
| CPU and shader checks | Production reservoir math: 250,000 trials; actual DDGI scheduler: eight cases; receiver-plane correction: 648 independent comparisons; full SPIR-V and DXIL shader validation each pass with 91 sources, 56 modules, 66 entry points and 74 cases |
| [Final DX12 low](../../logs/client/validation/lighting/lighting-dx12-low-z7z7zwdp/result.json) | 600 frames, 960x540, valid selected local shadow; 121 map updates, 5,454 raster draws, zero local RT rays, exit 0 |
| [Final Vulkan low](../../logs/client/validation/lighting/lighting-vulkan-low-i1tjeoop/result.json) | 600 frames, 960x540, valid selected local shadow; 112 map updates, 5,004 raster draws, zero local RT rays, exit 0 |
| [DX12 water/sky](../../logs/client/validation/lighting/lake-dx12-on-aphmd8vi/result.json), [Vulkan water/sky](../../logs/client/validation/lighting/lake-vulkan-on-psj2wl55/result.json) | 1,800 measured frames each, all 81 RT columns ready, zero native graphics warnings/errors; actual GPU captures visually inspected for water reflection/transmission, sky, white clouds and UI |
| [Final package DX12 high](../../logs/client/validation/lighting/lighting-dx12-high-y4y1g9rf/result.json), [final package Vulkan low](../../logs/client/validation/lighting/lighting-vulkan-low-bdo17tgr/result.json) | 600 frames each after preset/portability integration; 297,756 actual RT visibility rays and 2,042 valid probes on high; zero RT rays and valid cached local shadows on low; zero native graphics warnings/errors |

The lighting-only canonical `octaryn_client_bundle` build passed with no compiler warnings
in [its build log](../../logs/build/lighting-final-package.log). Native and packaged
client executable SHA-256 at that qualification both equal
`94725C80447E93EA79815741173EA4D1A08AD2F4366411CE3AF37979271DFD47`.
The packaged shader tree matches the source, including removal of DDGIReady.
Reproduce the build with `./tools/build/windows.ps1 -Action build -Target octaryn_client_bundle -Jobs 8`.

The subsequent coordinated cleanup integrated commit `bda959900b64f5eb12b11003b5d89fffe928f4dd`
into the working files while retaining the uncommitted lighting/UI changes. It removed
the disconnected GFX/probe/bootstrap paths and unused dependency fetches, and included
the release task's Linux host and authoritative fluid-scheduling fixes. `octaryn_all`,
`octaryn_validate_static` and `octaryn_validate_cpu` passed together in
[the combined build log](../../logs/build/lighting-cleanup-verified.log), including
35,529 fluid checks. The independent CPU draw oracle now inventories the actual
52-byte per-slot render targets and still verifies cached totals after mutations.
The combined package SHA-256 is
`4C6C259305A7C6B60C8AF2AA9AAF905F54D765D263F893167F719E3B569B2BEC`.
Its [DX12 high qualification](../../logs/client/validation/lighting/lighting-dx12-high-cf561gyb/result.json)
and [Vulkan low qualification](../../logs/client/validation/lighting/lighting-vulkan-low-hey2jacb/result.json)
each passed 600 frames without native graphics warnings/errors. Source reconciliation is retained in
[the recovery snapshot](../../logs/build/cleanup-integration/before.zip);
360 historical build-root files/directories were preserved under `logs/build/retained-build-root`,
with [a relocation manifest](../../logs/build/cleanup-integration/relocated-build-artifacts.json).
The tagged pre-RT preview was not changed by this local lighting integration.
The combined local client and server were then launched with the saved revision-2
world and radius-32 setting. D3D12 initialization, authoritative player readiness,
ongoing rendered frames and incremental RT/DDGI streaming are recorded in
[the launch log](../../logs/client/lighting-cleanup-launch.log). This startup is
not an additional completed maximum-distance or sustained-travel qualification.

DX12 1440p median GPU milliseconds in this fixture: DDGI trace/update 0.034/0.015;
ReSTIR initial/temporal/spatial/visibility 0.343/0.867/0.781/1.032;
sun trace/filter 0.117/0.202; composition 0.640. These are individual pass timings
for the bounded three-light fixture, not frame-rate guarantees or huge-light-count
results. Both high-tier captures reported 2,048 active and zero sleeping/inactive probes;
classification code existing does not qualify every state transition visually.

Remaining limits:

- Forward player/items, clouds and transparent surfaces are not deferred light
  receivers or dynamic AS instances. Existing forward/water presentation remains;
  full moving-object shadow/GI parity is not claimed.
- One DDGI volume, no cascades, coarse diffuse visibility, possible thin-wall
  leaking and temporal response delay. Probe markers are surface-based, not full
  free-space spheres. Radiance clamping adds bias.
- Uniform ReSTIR proposals/correlations can converge poorly with heterogeneous
  high light counts. There is no independent local radiance denoiser.
- Sun composition suppresses metallic diffuse but adds no metallic sun specular
  or new general reflection system.
- Three directional clipmaps have finite coverage. Local fallback shadows only
  one point/spot light and its bounded range.
- No Metal, Linux, macOS, NVIDIA or Intel runtime verification is claimed.
  Portable shader output and vendor-neutral APIs do not prove those platforms.
- Tests do not establish radius-128 streaming, high-speed travel, arbitrary-world
  performance, or every alpha/fluid asset's visual parity.

### Release inspection: diffuse sky energy correction

Fresh-world release inspection exposed a strong cyan/blue DDGI footprint.
The miss shader treated the stylized display sky as tone-mapped physical
radiance and applied `sky / (1 - sky)`, amplifying its saturated blue channel
before clamping it to 8. This made the finite probe volume much brighter and
bluer than the surrounding reduced ambient; passing graphics validation did
not detect the visual defect.

`DDGIEnvironment.slang` now defines linear diffuse environment radiance
separately from the display sky. Restrained directional/day/night/twilight tint
preserves the existing unoccluded ambient luminance and user intensity. It adds
no sun disk. Probe integration still stores pi times the cosine-weighted mean,
and the Lambertian resolve still divides irradiance by pi. Ray queries,
occlusion, direct-light visibility, emission and indirect bounces remain active.
The volume boundary now uses a two-cell smooth fade rather than a one-cell
linear fade, while retaining full coverage in the interior.

The earlier `4C6C2593...` executable hash alone cannot identify this correction:
Slang shader files are loaded at runtime. Earlier blue captures are superseded
for visual acceptance. The corrected bundle passes ordinary terrain and 600-frame
DX12/Vulkan high lighting runs under `logs/client/validation/lighting-release-20260914/corrected/`.
Inspected captures show restored material colors with active probes and zero
graphics warnings/errors. The final release qualification JSON and per-file
manifest identify the complete shader payload as well as the executable. The earlier runtime counters and
API checks remain evidence only for their recorded payloads.

## 14. Performance-sensitive areas

Full-resolution reservoirs and sun histories dominate added memory at 1440p.
Temporal/spatial bandwidth exceeds probe cost in this fixture. Procedural
queries reconstruct exact geometry/materials, so vegetation and high quad
counts increase traversal/alpha work. AS jobs are bounded but streaming can
temporarily exceed ready RT coverage. Raster sun draws off-camera columns in
three levels; selected local shadow refresh can draw six faces. Cache behavior
and high-light-count sampling need representative-world measurements.

## 15. Next improvements

1. Expand local-shadow tests to adversarial light switches, edits, shallow angles
   and cube-face seams beyond the qualified bounded fixture.
2. Reduce reservoir/history memory and bandwidth while preserving estimator
   checks and disocclusion rejection.
3. Add clustered/importance-based high-quality candidate proposals with correct
   PDFs, then measure large heterogeneous light populations.
4. Integrate dynamic player/items as explicit AS instances and light receivers
   before claiming moving-object lighting parity.
5. Add DDGI cascades and stronger relocation/thin-wall scenes, followed by
   separate Metal/Linux and additional-vendor qualification.

Primary references: [Slang RHI](https://github.com/shader-slang/slang-rhi),
[DDGI irradiance fields](https://jcgt.org/published/0008/02/01/),
[production probe extensions](https://jcgt.org/published/0010/02/01/), and
[ReSTIR DI](https://research.nvidia.com/labs/rtr/publication/bitterli2020spatiotemporal/).
The repository's pinned headers, not guessed newer APIs, define the implementation.
