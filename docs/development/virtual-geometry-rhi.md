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
