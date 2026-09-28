# Mesh shader comparison path

Set `OCTARYN_CLIENT_MAP_DRAW_MODE=meshlet` or pass `--draw-mode meshlet` to
`tools/validation/capture_map_world.py` to select the comparison path. The device
must expose Slang RHI's `MeshShader` feature; an unsupported explicit selection
fails initialization. Ordinary rendering keeps its existing draw mode.

Meshoptimizer builds opaque and alpha-tested meshlets separately for each
material. Each meshlet contains at most 128 vertices and 256 triangles, using a
128-thread group. The Slang mesh shader culls conservative bounding spheres
against the camera frustum, reads the original full-detail vertices and emits
the existing `MapSurfaceOutput`. The existing G-buffer fragment shader still
evaluates textures, alpha tests, normal maps and material features. Transparent
surfaces continue through the globally sorted forward pass. Ray tracing uses
the unchanged full-detail geometry.

The starting group and output limits follow
[AMD's mesh shader optimization guidance](https://gpuopen.com/learn/mesh_shaders/mesh_shaders-optimization_and_best_practices/).
They are comparison settings, not a universal optimum. This version uses no
amplification stage or meshlet LOD. It is deliberately not selected
automatically until matched GPU measurements and image comparisons support a
capability-specific choice.

Backface cluster culling uses the meshoptimizer normal cone stored per meshlet
(80-byte records): `dot(normalize(apex - camera), axis) >= cutoff` culls, per
the meshoptimizer header documentation and Real-Time Rendering 4th ed. 19.3.
Double-sided materials keep a degenerate cone (axis 0, cutoff 1) that never
culls, matching the fragment shader's per-material double-sided rule. The
camera-at-apex case is safe because NaN comparisons evaluate false. Cone
culling defaults on within meshlet mode; `OCTARYN_CLIENT_MAP_CONE_CULLING=0`
disables it. DX12/RX9070XT native1440 evidence (352 matched frames per variant,
`logs/client/cone-culling/`): opaque GPU 1.560→1.556 ms mean; captures differ
at the run-to-run temporal noise floor (MAE 0.42 vs 0.40 off/off control,
large-diff pixels 3985 vs 3411 of 3.69M, edge speckle only). The static street
view culls few backfaces; gains are view-dependent.

Monolithic maps prepare/upload meshlets during startup. Tiled maps prepare them
on asset workers, include them in the retained CPU budget, and upload the three
buffers through the normal staged byte/time-budget loop. Pipelines are prewarmed
before gameplay. Once uploads complete, CPU geometry and meshlet payloads can be
released independently of collision owners.

`tools/validation/validate_map_meshlets.py` exercises the production builder and
checks exact triangle multiplicities, material boundaries, winding, local index
ranges and conservative bounds. DXIL and SPIR-V shader compilation only prove
compiler acceptance. DX12 and Vulkan runtime output, timing and memory still
require separate capture evidence.
