# Virtual geometry RHI foundation

The implementation retains Slang SDK 2026.17.1 and standalone slang-rhi commit
`e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc`. `tools/build/slang-rhi.py` registers
the ordered patch stack; the build receipt records the full stack hash. Existing
patches are preserved. The four new patches are first-party changes to the
Apache-2.0-with-LLVM-exception dependency; its license remains with the source.

- `slang-rhi-mesh-indirect-api.patch`: typed command recording, resource retention,
  public indirect mesh API, and argument/count-buffer validation.
- `slang-rhi-mesh-indirect-backends.patch`: DX12 command signatures and Vulkan EXT
  mesh dispatch/count entrypoints; standard buffer int64 atomic capability.
- `slang-rhi-mesh-indirect-contract.patch`: explicit unsupported-device/count-limit
  errors and packed argument documentation.
- `slang-rhi-mesh-validation.patch`: typed null root SRV/UAV bindings for unused
  reflected parameters; Vulkan indirect resource transitions before rendering.

`IRenderPassEncoder::drawMeshTasksIndirect(maxDrawCount, argBuffer, countBuffer)`
uses packed 12-byte dispatch records and an optional uint32 count at its own byte
offset. `MeshShaderIndirect` includes multi-draw/count support. `AtomicInt64Buffer`
means standard buffer atomics; on DX12 the guaranteed binding is a root UAV with
SM6.6 and `Int64ShaderOps`. It does not promise typed-texture, shared-memory, or
descriptor-heap atomics. The root attribute name is configured through
`D3D12DeviceExtendedDesc::rootParameterShaderAttributeName`.

The implementation follows the public
[DX12 mesh shader specification](https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html),
[DX12 atomic specification](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Int64_and_Float_Atomics.html),
and [Vulkan EXT mesh shader specification](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_mesh_shader.html).
It does not depend on NVAPI atomics or NVIDIA mesh extensions. Metal mesh execution
is outside this change; its backend explicitly reports the new command unsupported.

## Verification

The native dependency rebuilt with both DX12 and Vulkan. Exact patch-stack checks
passed before each build. The RX 9070 XT headless probe exercised GPU-generated
arguments, independent argument/count offsets, zero count, zero dispatch, maximum
count clamping, and 64-bit root-buffer atomic minimum/maximum winners on both APIs.
Enabling **API core validation**, beyond the RHI debug wrapper, exposed the null
root descriptor and Vulkan in-rendering barrier bugs; the validation patch fixes
both. Subsequent core-validation runs passed these cases and the hybrid geometry
fixture. DX12 redundant-transition warnings in selection remained a separate
follow-up at the time of this note. This evidence does not establish NVIDIA,
Intel, Linux, or production scene performance.

## Animation companion

The separate Animation owner follows the public
[glTF 2.0 interpolation and skinning specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
It imports triangles with authored normals, up to eight joint influences, morph
position/normal/tangent deltas, hierarchy/inverse-bind matrices, and STEP, LINEAR,
and CUBICSPLINE channels. CPU sampling clamps at key boundaries, uses quaternion
slerp for LINEAR rotations, and normalizes cubic quaternion interpolation without
changing tangent signs. The original deterministic importer fixture has no third-party
asset licensing dependency.

The Slang deformation pass writes current/previous object-space positions,
inverse-transpose normals, mirrored tangent handedness, and their union bounds.
The owning caller supplies clip time, matching palettes, command submission, and
resource retirement. Static map import remains unchanged; these APIs and focused
probes do not establish animated world rendering or animated ray-scene integration.

## Required map ray geometry (2026-09-30)

Production map reflections and shadows now consume `WorldGeometryRay` snapshots
derived from the same paged geometry asset as opaque rendering. Selection keeps a
complete cut of every root, including geometry outside the camera frustum. GPU
extraction validates source page generations and creates immutable ray buffers;
later raster page eviction cannot change a published ray scene. BLAS batches
retain a per-triangle material lookup, so mixed materials and alpha masks are
resolved with their authored textures and factors rather than one material per
BLAS. BLEND still uses the separate sorted forward presentation pass.

Ray expansion stores authored position, normal, both UV sets, tangent and color
as 72 bytes per vertex; only unused padding is removed from `MapVertex`.
POSITION-only source clusters use 12-byte vertices and derive flat triangle
normals at the material hit. Mixed cuts use the authored representation. The
160-byte material/instance descriptor has explicit padding and a vertex stride,
with matching DXIL and SPIR-V offsets. Original node transforms share object-space
BLAS buffers. Ray material evaluation applies normalized inverse-transpose
normals, orthogonalized tangents and mirrored handedness. Changing the resident
node membership increments a revision that invalidates cached TLAS reuse and
current-coverage checks, even when the map and BLAS pointers remain unchanged.

Large builds submit bounded groups of BLAS builds and compact copies with shared
scratch. Original structures remain alive until their copy fence completes; a
replacement is published only after every batch and its TLAS complete. The
current per-owner limits remain at most 512 MiB for building and 1 GiB for
resident snapshots, including retained frame consumers. A final uncompressed
remainder may be retained when it fits; compaction never requires exceeding the
same peak budget. Tile/catalog admission reserves this ray capacity in addition
to raster, material and forward capacity.

An unsuccessful refinement keeps the previous complete snapshot. A stationary
camera does not retry that rejected build merely because additional raster
pages arrive. A metre of camera travel, a 10% focal change, or a 25% change in the
published cut's projected error allows a new admission decision. Before that
decision, older frame snapshots must retire so their temporary overlap cannot
permanently block refinement. `error_pixels` in the ready marker is the actual
conservative bound of the published cut; `requested_pixels` records the desired
selection threshold. Broad bounds containing the camera can make the former
very large. A complete cut is a coverage guarantee, not a one-pixel quality claim.

The RX 9070 XT headless GPU probes passed on Windows DX12 and Vulkan with authored
attributes, POSITION-only normals, mixed materials, alpha masks, mirrored and
nonuniform instances, dynamic-item offsets across several map BLAS, offscreen
coverage, staged publication and retained consumer fences. Logs are under
`logs/tools/zorah-vg/gpu-{dx12,vulkan}-stride.log`. The first-party metallic-floor
fixture also produced red and green reflections from panels outside the actual
90-degree camera frustum, with no blue contribution from its fully masked panel;
turning reflections off removed those colored pixels. Those fixtures qualify
the tested material and coverage cases, not arbitrary scene quality or another
GPU/platform.

The packaged DX12 Bistro capture in
`logs/client/zorah-integration/bistro-final/map-dx12-on-0-r_6q0vrc` passed the
unchanged 50 ms sustained-frame watchdog and exited normally. It published two
complete ray snapshots; the second used 534,648,592 bytes and reported an actual
conservative error bound of 4,902.78 pixels against a requested threshold of one.
At the configured 30 FPS cap, 356 measured frames had total mean 33.766 ms,
p99 35.546 ms, worst 45.114 ms, and renderer CPU mean 1.366 ms. The retirement wait
allowed the next replacement attempt after old frame owners released; its actual
build-budget rejection retained the complete second snapshot without repeated
stationary attempts. This is fixed-camera capped evidence, not an uncapped or
moving-camera performance result.

The actual mirrored SceneLoader view at camera x=16 in
`logs/client/zorah-integration/scene-mirrored-view-final/map-dx12-on-0-zlbw93ds`
passed with 5,428 red and 5,976 green reflection pixels and zero masked-blue
pixels. This complements the shader/probe transform checks with the packaged
scene-loader path. Full Zorah coverage, animated ray integration, and
Linux/macOS/Metal runtime qualification remain outstanding.

The staged `SceneSession` implementation keeps incoming parts outside the active
map list until roots and requested ray snapshots are complete. Pending uploads
use the normal frame encoder and fence, including abandoned resize frames;
promotion occurs before staging a new frame so one stream cannot record both
pending uploads and raster selection twice. Collision-only startup records no
geometry uploads. Pending ray completion does not invalidate active reflection
history before membership publication. Existing exact TLAS identity checks remain
in force, including instance membership revisions.

Active and pending owners retain their full reservations. Removed owners leave
active membership immediately, allowing old frame/TLAS references to retire;
new allocation waits while those retired reservations occupy the budget. Keeping
old membership indefinitely would deadlock this handoff. Opt-in
`OCTARYN_CLIENT_SCENE_CONTINUITY=1` emits publication, per-owner retirement fences,
and per-submitted-frame reflection/coverage and allocated reservation evidence.
The desired plan reservation can exceed free capacity while allocation waits;
`allocated_reservation_bytes + retired_bytes` is the bounded owner charge.

The moving DX12 fixture passed in
`logs/client/zorah-integration/scene-continuity/scene-stream-lgd3lulr/result.json`
(capture case `map-dx12-on-0-9478kqkw`). Authority travelled 259.878 metres with
0.122 metre endpoint error and no blocked movement samples. Frames 199 through
1707 supplied 1,509 consecutive records, including 13 incoming-part preparation
frames: rays stayed enabled, exact active-TLAS coverage stayed complete,
reflection history remained valid, and an actual reflection GPU pass executed
on every frame. Four incoming parts published only after root/ray readiness;
four old parts were collected only after their recorded upload/ray and frame
consumer fences completed. Peak active, pending and retired owner reservation
was 31,295,520 bytes within the unchanged 536,870,912-byte budget. Source and
cooked fixture hashes remained unchanged.

Before and after travel, inspected captures contained respectively 5,616/5,636
red and 5,624/5,616 green reflection pixels, with zero masked-blue pixels. The
colored panels remained outside the actual 90-degree camera frustum; their
authored reflected hits were below 8.67 metres with a 64-metre reflection range.
Each requested 128-metre region was complete, while the distant station remained
explicitly outside global manifest readiness. This run used full-detail geometry
selection (`--geometry-pixels 0`) and retained the 50 ms watchdog. It qualifies
publication and reflection continuity in this small prepared DX12 fixture;
it does not establish arbitrary global coverage, moving LOD quality, Zorah-scale
admission/performance, or Vulkan moving-scene continuity.
