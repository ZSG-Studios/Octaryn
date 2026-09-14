# First hardware ray-tracing checkpoint

2026-09-14, Windows / RX 9070 XT. This records the first validated RT pass.
The separate modern-lighting task is extending the renderer with DDGI, tiled local lighting
and shadow filtering. Its subsequent build must be qualified separately; the
results below do not establish those additions.

## Behavior

Slang inline RayQuery runs through standalone slang-rhi on DX12 and Vulkan.
Exact packed voxel quads generate procedural BLAS bounds on the GPU. Queries
intersect the actual two triangles, never accept bounding boxes as surfaces.
Raster and rays share sprite, torch and fluid corner decoding. All resident
columns participate, including columns outside the camera frustum. One column
build is pending at a time; zero-timeout fence polling publishes completed
builds. Camera motion does not rebuild unchanged acceleration structures.
Immutable snapshots retain geometry across two frames in flight. Empty scenes
use a real masked instance because the pinned RHI requires a nonempty TLAS.

Terrain sunlight is visibility-tested before HDR compositing. Ambient/emission
remain separate. Water reflects actual terrain hits and their atlas material,
and samples atmosphere on misses. Traced HDR radiance converts explicitly into
the existing bounded water color domain. G-buffer positions use RGBA32Float to
avoid distant half-precision shadow-origin errors; visibility uses a per-frame
R32Float target.

Graphics has persistent Ray tracing On/Off, default On, and disabled Unavailable
on unsupported devices. Settings version 10 stores `rayTracingEnabled` and
preserves earlier defaults. Diagnostic environment modes are
`OCTARYN_CLIENT_RAY_TRACING=auto|off|required`.

This pass traces terrain, plants and solid lava. Water/glass transmit these rays
without multi-bounce transmission. Player/item meshes, clouds as reflected
geometry, GI and soft-shadow filtering are not claimed here. The shared decoder
includes slopes/cutouts; the initial analytic ray oracle specifically qualifies
opaque cubes and lifetime cases, not every material.

## Color and cloud presentation

The [jitter and color repair](cloud-water-jitter.md) remains: correct FSR
projection jitter, vibrant azure sky and cobalt lake water. Clouds now multiply
authored radiance by a bounded daylight factor up to 4 before tone mapping.
It ramps over sun elevations 0.62–0.82, preserving dusk/night, transparency and
face shading. Display-white previously entered HDR near 1 and tone-mapped gray.

Native captures were visually inspected on both APIs. Clouds are white and
water blue; timber posts appear in reflections with RT On and disappear with
RT Off. Cloud locations differ between APIs due to existing floating-point
noise; pixel-exact cross-API cloud parity is not claimed.

## Evidence

Frozen validation bundle: `build/release-windows/client/ray-baseline-qualified`.
Client SHA256:
`5fc34f65c61f1a676318819dcf76c07f17a06db6a5f879ea4229e1615266a22e`.
Runtime content digest (156 files):
`ff470ffad49cf60e7ae422aeb34fd9158320a387ef95a7a874a161127cf3a7fa`.
The final UI stylesheet was staged after the native build; this digest includes it.

Under `logs/client/validation/ray-tracing`:

- `dx12-native.log`, `vulkan-native.log`: real hardware queries, signed hit
  distance/normal/material, finite misses, offscreen hits, zero camera rebuilds,
  immediate eviction with two retained frames, edit/reload/edited-air all pass.
- `lake-dx12-on-_qppt0u7`, `lake-vulkan-on-rb8sr9h0`: production client, Native AA,
  1,800 frames, 81 ray-ready columns, 321,436 quads, native validation and captures.
- `lake-dx12-off-exzdxgb6`: matching fixture with reflections disabled.
- Native UI passes 703 checks at 640×480, 1280×720, 960×640 and 1920×1080. The
  wide/short menu needed bounded scrollbar sizes outside the narrow-screen
  media query; assertions were preserved.
- `logs/build/retained-build-root/ray-tracing/fix-build.log`: settings probe passes 62 checks, including
  defaults, Boolean validation, round-trip and capability separation.

Use `tools/validation/validate_ray_lighting.py` for isolated lake captures and
the staged world-mesh probe's `--ray-tracing-only` for native ray contracts.
Production Shadow and water query entry points compile to DXIL and SPIR-V;
Shadow also emits Metal source. Metal runtime remains untested.

## Streaming cost

The first run uses 2560×1440, FSR Quality, radius 32, two frames in flight,
no LOD, 32 m/s camera/stream movement for 15 seconds (480 m); the authoritative
player stays stationary. Startup and final settle require complete ray coverage
as well as raster residency, preventing incomplete RT from inflating results.

`performance/streaming-dx12-quality-csjjjqnk/result.json` passes. Moving frames:
11.035 ms mean (about 90.6 FPS), 27.278 ms p99, 143.673 ms maximum. There were
33 frames over 16.67 ms and three over 50 ms. Residency reached a minimum of
4,082 columns and returned to all 4,225 columns / 15,573,564 quads.

Acceleration structures use about 3.28 GB at this exact geometry count. Reported
total GPU bytes are 4,226,993,523; the first counter omits some evicted mesh buffers
retained by RT snapshots. Accounting correction and buffer reuse are handed to
the modern-lighting owner. Cold load built 8,125 BLAS because neighbor arrival
remeshes existing columns. Allocation/submission spikes remain. Concurrent build
tasks were active in this workspace; these are observed run timings, not isolated
hardware measurements or a controlled before/after speedup. This is not a claim
of stutter-free RT or unchanged performance relative to RT Off.

The attempted same-bundle Off comparison at
`performance-off/streaming-dx12-quality-1tdtltmb` failed the strict moving-interval
gate despite normal client exit and complete final geometry. It includes a
1.437-second outlier. The failed evidence is retained; no qualified Off comparison
or RT-versus-Off speed ratio is reported.

## References

- [Slang RayQuery](https://docs.shader-slang.org/en/stable/external/core-module-reference/types/rayquery-03/)
- [Standalone slang-rhi](https://github.com/shader-slang/slang-rhi), pinned local
  acceleration-structure API and `tests/test-ray-tracing-common.h`.
- [DXR inline tracing](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#inline-raytracing)
- [AMD RDNA performance guidance](https://gpuopen.com/learn/rdna-performance-guide/)
