# Local direct lighting and ReSTIR DI

The client owns `ReSTIRDISystem`, `WorldLocalLight` and its GPU buffers. Game code
publishes point, spot or rectangle lights with `open_world_renderer_set_lights`.
The setter validates finite values and bounds the list to 65,536 lights. Ordinary
startup creates no synthetic lights. `OCTARYN_CLIENT_LIGHTING_FIXTURE` is an
explicit qualification fixture only.

The implementation follows the reservoir equations in
[Bitterli et al., 2020](https://research.nvidia.com/labs/rtr/publication/bitterli2020spatiotemporal/).
Each initial light is sampled uniformly with proposal probability `1/lightCount`;
rectangle parameters are uniform over a unit square and their area Jacobian is
included in `local_light_radiance`. Candidate weight is `target/proposal` and the
resolved multiplier is `sumWeights/(M*selectedTarget)`. Reuse re-evaluates the
target at the destination surface, then merges with weight
`destinationTarget*sourceMultiplier*representedCount`. Reducing represented count
also reduces its weight; clamping M alone is incorrect.

The target has a positive numerical floor even for lights that contribute zero
at a particular surface. This maintains common light-domain support during reuse.
Visibility is excluded from target evaluation and traced only for the final
selected contributing sample, so candidates do not each require a shadow ray.
Temporal history receives temporal-only reservoirs, avoiding recursive spatial
feedback. This does not eliminate every source of sample correlation or variance.

Pass order is initial sampling, camera/world-position reprojection, spatial reuse
and selected visibility/resolve. History validates world position, packed voxel
identity and geometric normal. Camera cuts, extent changes, light-list revisions
and centralized scene-change revisions invalidate history. Reprojection includes
the previous frame's actual raster jitter. The current opaque voxel G-buffer does
not include animated character surfaces or a sampled normal-map attachment.

Low/medium use GPU tile lists. One thread per light projects its bounded influence
into 16x16 screen tiles; the resolve visits at most the configurable list capacity
(64 by default). Point and spot lights in a non-overflowing tile are summed
deterministically. Rectangles use one stochastic area sample per light. Overflow
tiles use the full-support reservoir estimator, retaining lights beyond the list
capacity. `LocalShadowSystem` selects the point or spot light with greatest
camera-relative importance and renders six retained perspective depth maps, 256px
per face by default. The maps use the authoritative voxel geometry, alpha-tested
sprites and lava surface heights. Water and glass transmit. The resolve uses
receiver-plane-corrected 3x3 PCF for that light in both ordinary tiles and the
overflow estimator. Geometry/light/config changes invalidate the maps; static maps
are reused. Other lights and rectangles remain unshadowed, as do surfaces beyond
the configurable local shadow range (64m default, never exceeding the light's
influence). The directional raster fallback has a separate owner.

The shared `LocalLight.slang` representation and incident-radiance function also
serve DDGI hit lighting. Direct light reflected at a probe-ray hit represents a
bounce at the final camera surface; composition adds local direct light separately.

Retained resource layout:

| Resource | Layout |
| --- | --- |
| Light input | 80 bytes/light, position/range, color/intensity, direction/cone, rectangle axes/type |
| Initial, temporal, spatial, history reservoirs | 32 bytes/pixel each, light, sample UV, sum, target, M, age, reuse flags |
| Surface history | 32 bytes/pixel, world position, packed voxel, normal, valid flag |
| Composition output | RGBA16F, 8 bytes/pixel |
| Tile counts and lists | uint count/tile plus bounded uint light indices |
| Selected local fallback shadow | Six D32 textures, 256x256 default; 1.5 MiB total |
| Diagnostic counters | Four uints: actual selected visibility rays, shaded reservoirs, temporal and spatial acceptance |

The full reservoir state is 168 bytes/pixel before light/tile buffers (about 591 MiB
at 2560x1440). It is allocated on demand when lights exist. Empty startup retains
only the composition texture and a dummy light binding. RHI command buffers retain
bound resources, so replacing resized or changed buffers does not release resources
still referenced by submitted commands. No normal rendering readback is added.

Every ReSTIR pass has named GPU timestamps. Debug modes expose selected light,
age, M, temporal/spatial acceptance, visibility, sum of weights and light count.
Counters reduce through group shared memory with four global atomics per group;
the fallback does not require subgroup/wave operations.

Verification performed during implementation: all ReSTIR and tile-list shader
entry points compiled for SPIR-V and DXIL with the pinned Slang 2026.17.1 compiler.
The portable fallback also emitted MSL, which is not a Metal runtime result.
`tools/validation/restir_math.cpp` compiles the exact production reservoir module
as C++ and passed 250,000 deterministic trials for weighted selection, target
re-evaluation and count-limited merging. The integrated production harness is
`tools/validation/validate_lighting_architecture.py`; retained runtime results and
GPU captures must be reported separately from shader/math checks.

The lighting harness requires the explicit capture's GPU counters to show actual
reservoir shading and enabled temporal/spatial reuse. High/ultra additionally
require visibility rays and valid DDGI probes. Low/medium require a valid selected
point/spot shadow, depth-map updates and raster draws, with zero local hardware RT
rays. `--resize` runs the existing nine-phase production FSR/mode/window-size
qualification with lighting enabled and verifies its actual render/output extents,
history resets and final capture. It does not inject OS input. The default fixed
case allows 600 frames; capture waits for completed RT coverage and a stable scene.

Remaining performance work includes smaller reservoir storage, light importance
sampling or cluster-assisted high-quality candidates, and measurements with many
lights. The RT local resolve rejects selected distances beyond the RT scene's
configured trace range rather than claiming untraced visibility. Spatial and
temporal reuse stabilize selection but are not a separate radiance denoiser.
