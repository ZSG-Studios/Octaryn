# Tiled local direct lighting

The client uses `LocalLightingSystem` to evaluate the contributing point, spot,
and rectangle lights in each screen tile. It runs through Slang and standalone
Slang RHI. The local-light implementation has no stochastic light selection or
light-selection history. DDGI remains the separate indirect-light system.

## Source and reference

The organization follows the screen-tile culling principle demonstrated by
[AMD's TiledLighting11 documentation](https://gpuopen.com/learn/tiledlighting11-directx-11-sdk-sample/)
and [reference source](https://github.com/GPUOpen-LibrariesAndSDKs/TiledLighting11).
AMD demonstrates both Forward+ and tiled deferred lighting. Octaryn retains its
own deferred voxel surfaces, exact geometry, Slang shaders and RHI backend.

Production owners are `LocalLightingSystem.h/.cpp`, `ClusteredLocalLights.cpp`,
`LocalLight.h`, and `LocalShadowSystem.h/.cpp`. The shaders are
`ClusteredLocalLights.slang`, `ClusteredLocalSort.slang`,
`ClusteredLocalResolve.slang`, `LocalDirect.slang`, and `LocalDirectRT.slang`.
Shared material evaluation and emitter-aware visibility retain their existing
owners in `LocalLight.slang`, `LocalLighting.slang` and `LocalVisibility.slang`.

## Evaluation and visibility

GPU culling builds lists for 16x16 screen tiles. Each tile stores up to 64 light
indices by default, with a maximum configurable capacity of 256. The bounded
list is sorted by ascending light index before shading, so parallel append order
does not change accumulation order between frames. A saturated tile evaluates
the entire light list in ascending order. This preserves all contributors;
large overlapping light populations can cost substantially more GPU time.

Point and spot lights use one deterministic evaluation. Rectangle lights use
four fixed area samples. High/ultra with ray queries test visibility separately
for each contributing sample against the shared world acceleration structure
and animated player shadows. Several contributing lights can legitimately
produce several visibility rays per shaded pixel. Material alpha cutouts,
water/glass transmission, and exclusion of the emitting voxel use the same
query rules as other lighting effects.

Low/medium use the same deterministic sums with the existing selected local
raster shadow cache. One important point or spot light receives six retained
perspective depth maps with receiver-plane-corrected PCF. Other lights,
rectangle lights, and samples beyond that cache's range remain unshadowed on
this fallback path. Sun shadowing has a separate owner.

Game code publishes lights through `open_world_renderer_set_lights`, bounded to
65,536 combined entries. Resident torch and exposed lava sources are extracted
from immutable column data and updated on authoritative publication. The block
source selection remains bounded to 4,096 nearby emitters. Ordinary startup
creates no synthetic API lights; `OCTARYN_CLIENT_LIGHTING_FIXTURE` is diagnostic.

DDGI uses the same light representation at probe-ray hits. Direct illumination
reflected at a ray hit becomes a bounce at the camera surface, while HDR adds
local direct lighting at that surface separately. The shared light model does
not make the two contributions duplicates.

## Resources and diagnostics

| Resource | Layout |
| --- | --- |
| Light input | 80 bytes/light |
| Tile counts and sorted indices | 4 bytes/count plus 4 bytes/index; bounded capacity per 16x16 tile |
| Direct output | RGBA16F, 8 bytes/pixel; 28.125 MiB at 2560x1440 |
| Raster local shadow cache | Six D32 faces for one selected light, 256x256 default |
| Diagnostic counters | Four uints: visibility rays, shaded pixels, evaluated lights, overflow pixels |

Direct local lighting writes its current-frame result without temporal or spatial
filtering. There is no stochastic light-selection noise or local-light history
lag; fixed area-light quadrature still approximates soft shadows. DDGI filtering
and sun-shadow temporal antialag remain separate. Submitted RHI commands retain
resized buffers/textures until completion. Diagnostic readbacks occur only after
explicit capture fences.

`LightingProfile` records `local_cull_ms` (including sorting) and
`local_shade_ms`. Captures expose `local_visibility_rays`, `shaded_pixels`,
`evaluated_lights`, and `tile_overflow_pixels`. Overflow counts shaded pixels
whose tile exceeded capacity, rather than unique tiles.

`validate_lighting_architecture.py` requires nonzero shaded/evaluated work,
actual RT rays for high/ultra, and valid DDGI probes when DDGI is enabled. It
accepts more rays than pixels and rejects more than four rays per evaluated
light. Low/medium require zero local hardware rays and actual selected-shadow
map updates/draws. Profile validation requires finite nonzero culling, shading,
sun and composition work. `validate_lighting_motion.py` additionally checks both
frame-resource slots, digging/replacement, and optional DDGI isolation.

## Qualification boundaries

Compilation, counter validation, runtime graphics validation and inspected
images are separate checks. Historical lighting captures in
[the architecture report](lighting-architecture.md) and
[cave repair report](cave-lighting-generator-repair.md) predate this direct-light
replacement and qualify only their recorded executable/shader payloads. Fresh
RT/raster runs and dense-tile checks are required for the current implementation;
old timings do not establish its cost. Fixed rectangle quadrature approximates
area-light integration and does not eliminate shadow undersampling. Windows
results do not establish Linux hardware Vulkan, macOS Metal, or other vendors.
