# Godot FSR2 port assessment

Assessed 2026-09-13. Source inspection only; no engine implementation, dependency
change, build, or GPU validation was performed for this assessment.

## Conclusion

Feasible, with substantial renderer integration. Godot provides a reusable FSR2
integration design, not a standalone library that brings its three backends into
Octaryn automatically. Keep Octaryn's C++ and standalone Slang RHI ownership.
Port Godot's RenderingDevice adapter to RHI and preserve the FSR2 algorithm and
Godot patches. Do not import the Godot engine or introduce a second graphics API.

Native 100% AA is the first useful milestone, followed by upscaling. Both require
correct temporal inputs. Completion means actual Vulkan, DX12 and Metal execution
with the same algorithm, not a Vulkan build and a theoretical Metal path.

## Exact reference

- Godot master inspected at `2f698aa5fe31d0be68f205ec41aec9365081d364`.
- Its vendor manifest records FSR **2.2.1**, upstream
  `1680d1edd5c034f88ebbbb793d8b88f8842cf804` (2023), under MIT.
- This is the version in the current Godot integration; it is not a claim that
  2.2.1 is AMD's newest FSR revision. Do not silently substitute FSR3 or another
  FSR2 revision when reproducing this integration.
- Godot records `0001-build-fixes.patch` and `0002-godot-fsr2-options.patch`.
  The inspected tree also contains `0003-storage-format-fix.patch`; the manifest
  alone is not a complete patch inventory.
- The current adapter is roughly 880 C++ lines plus a 196-line header. The vendor
  subtree is 49 files, about 0.8 MB; eight small GLSL pass wrappers sit outside it.
  The wrapper's size does not represent the whole algorithm or integration cost.
- Godot and AMD notices must accompany reused code. Keep vendor source pinned and
  separate from first-party code; split our adapter into focused files under the
  existing 500-line rule rather than copying Godot's large translation unit.

References:

- [Vendor manifest](https://github.com/godotengine/godot/blob/2f698aa5fe31d0be68f205ec41aec9365081d364/thirdparty/README.md#amd-fsr2)
- [C++ adapter](https://github.com/godotengine/godot/blob/2f698aa5fe31d0be68f205ec41aec9365081d364/servers/rendering/renderer_rd/effects/fsr2.cpp)
- [FSR2 sources and patches](https://github.com/godotengine/godot/tree/2f698aa5fe31d0be68f205ec41aec9365081d364/thirdparty/amd-fsr2)
- [Native-resolution AA documentation](https://docs.godotengine.org/en/stable/tutorials/3d/resolution_scaling.html)

## What the adapter must replace

Godot maps the standard FSR backend callbacks onto RenderingDevice. Our equivalent
must implement context creation/destruction, capability queries, resource
creation/destruction and registration, resource descriptions, pipeline
creation/destruction, and GPU job scheduling/execution. Replace Godot RID,
ShaderRD, uniform-set and compute-list handling with RHI resources, Slang
reflection/bindings and command encoders. Preserve mip views, samplers, constant
buffer lifetime, clears, copies and inter-pass ordering.

Godot's shaders use GLSL and its SPIR-V translation pipeline. Octaryn requires
Slang. Assess the matching AMD HLSL headers as the translation starting point,
carrying across Godot's patches and comparing behavior against the pinned GLSL
reference. This is not verified Slang compatibility yet. Validate FP32 first;
enable FP16/wave permutations only with compiler and device evidence.

Important patch details: Godot adds a reprojection matrix, derives camera motion
for invalid motion vectors and clamps the reactive mask to 0.9 in GLSL callbacks.
These changes are not automatically mirrored into the HLSL callbacks. Port the
CPU constant layouts and shader changes together. The storage-format correction
uses R32F for dilated depth and RGBA16F for prepared color; preserve those exact
formats rather than the older R16F/RGBA16 UNORM declarations.

Godot's dispatch uses a separately supplied reactive texture and disables automatic
reactive generation. Following that behavior is the simplest reference-faithful
starting point. Automatic generation is an optional alternative, with additional
opaque-scene capture requirements; it is not already handled by the wrapper.

Pass families are reconstruct previous depth, depth clip, lock, accumulation
(with optional sharpening), RCAS, luminance pyramid and optional reactive-mask
generation. Preserve their actual dependencies and synchronization.

- [Godot callback changes](https://github.com/godotengine/godot/blob/2f698aa5fe31d0be68f205ec41aec9365081d364/thirdparty/amd-fsr2/patches/0002-godot-fsr2-options.patch)
- [Storage-format correction](https://github.com/godotengine/godot/blob/2f698aa5fe31d0be68f205ec41aec9365081d364/thirdparty/amd-fsr2/patches/0003-storage-format-fix.patch)

## Current Octaryn mapping

Paths below are relative to the active repository root.

| Area | Current evidence | Required change | Difficulty |
| --- | --- | --- | --- |
| RHI dependency | `tools/build/slang-rhi.ps1:49` enables Vulkan; line 52 disables DX12 and Metal. `cmake/Dependencies/SlangSdk.cmake:21` rejects non-Windows builds. | Enable/package DX12 dependencies; provide native Linux/macOS builds and platform linkage. | High; independent of FSR |
| Device/compiler | `WorldRendererDevice.cpp:59` forces Vulkan and lines 61-62 force SPIR-V. `RhiShader.cpp:25` obtains the device's Slang session. | Select device and matching shader target per API; audit existing passes too. | Medium-high |
| HDR insertion | `WorldRenderer.cpp:52-72` composites opaque HDR, forward content, tone maps, then draws RmlUi. `Hdr/WorldHdr.cpp:17` creates RGBA16F scene with SRV/UAV/RT usage. | Insert FSR before tone mapping; present reads reconstructed HDR. Keep UI at display resolution, unjittered. | Low-medium |
| Depth | `WorldRendererDevice.cpp:41-45` creates D32 with DepthStencil usage only. | Add shader-readable depth/view; verify format/state support on every backend. | Medium |
| Depth convention | `WorldDraw.cpp:10-13` uses finite .1-to-8192 projection; frame clears depth to 1. | Configure non-reversed finite 0..1 depth. Do not copy Godot reverse-Z settings. | Medium |
| Camera | `WorldRenderer.h:16`, `WorldDraw.cpp:7-16` contain only current camera parameters. | Previous rendered camera, jitter, frame delta, projection and reset state; consistent jitter across all scene passes. | High |
| Motion | `WorldRaster.slang:100-105` has four outputs with no velocity. `PlayerRenderer.cpp:58-77` sends only current skin/body transforms. | Camera reprojection for static terrain plus actual object/deformation velocity for the player. | High |
| Transparency | `WorldRasterPipeline.cpp:29-35` disables depth writes for water/glass; clouds also do not write depth. | Reactive/composition masks for transparency, animated textures, lava and clouds. Capture opaque color if using automatic reactive generation. | High |
| Dimensions | `WorldRendererDevice.cpp:34-53` and `WorldRenderer.cpp:38-39` use window size for all scene targets and viewports. | Separate render dimensions from output dimensions; keep native mode at 1.0. | Medium |
| Lifetime | `WorldFrames.h` and `WorldTargets.h` contain 1/2 frame slots. | Chronological history separate from slot reuse; GPU ordering and retained descriptors/constants across submissions. | Medium-high |

Renderer paths without an owner prefix above are under
`octaryn-client/Source/Rendering/RenderBackend`; player paths are under
`octaryn-client/Source/Rendering/Player`. Shader paths are under
`octaryn-client/Shaders/Voxel`.

The current renderer has integer chunk/local geometry arithmetic but float camera
positions. It does not establish a complete large-world floating-origin protocol.
Reprojection must use consistent current/previous origins and preserve precision.
Ordinary chunk streaming and block edits should use disocclusion rejection rather
than continually resetting history for the entire screen.

Reset on new session, camera cuts/teleports, resolution/mode changes and resource
recreation; handle projection changes, minimize/resume and skipped frames. Commit
previous camera and skin state only when the frame has been submitted successfully.
Choose explicitly whether the selection outline remains temporal or becomes a
separate full-resolution overlay.

## Metal qualification risk

Godot's FSR2 reconstruct-depth shader uses R32UI image atomic min/max. Its Metal
shader compiler requires native 32-bit image atomics and checks GPU/MSL support;
the inspected implementation uses MSL 3.1 with macOS 14/iOS 17 availability gates.
This is a supported-device requirement, not universal support for every Metal GPU.
Slang must emit equivalent real atomics; replacing them with ordinary reads/writes
would be incorrect. Godot's successful translation does not prove Slang's path.

- [Godot Metal compiler gate](https://github.com/godotengine/godot/blob/2f698aa5fe31d0be68f205ec41aec9365081d364/drivers/metal/rendering_shader_container_metal.cpp#L248)
- [Device feature gate](https://github.com/godotengine/godot/blob/2f698aa5fe31d0be68f205ec41aec9365081d364/drivers/metal/metal_device_properties.cpp#L157)
- [FSR image atomics](https://github.com/godotengine/godot/blob/2f698aa5fe31d0be68f205ec41aec9365081d364/thirdparty/amd-fsr2/shaders/ffx_fsr2_callbacks_glsl.h#L513)

## Proposed stages and acceptance

1. Pin sources/notices and implement a minimal RHI FSR dispatch harness. Compile
   matching passes to SPIR-V, DXIL and MSL. Execute synthetic depth/velocity/history
   inputs and integer-atomic checks on Vulkan, DX12 and a supported Mac. This is
   the earliest useful gate for the user's three-API requirement.
2. Add temporal camera state, sampleable depth and static-terrain velocity. Run
   Native 100% on opaque terrain with history reset and 1/2 frames in flight.
3. Add previous player skin/body state, sky handling and transparency masks.
   Verify water, glass, lava, moving clouds and texture animation in motion.
4. Split render/output sizes; add scale modes and mip bias; keep all UI native.
   Measure pass timings, memory and image stability against Native AA.
5. Qualify the actual packaged game on all three APIs, including existing scene
   passes, resize, mode switch, teleports, camera changes, block edits, streaming,
   high-coordinate travel and long motion sequences. Record platform results
   separately. Shader compilation alone does not satisfy this stage.

Planning estimate, not a measured schedule: an opaque Vulkan Native AA prototype
is plausibly several focused engineering days. A complete scene/upscaling port is
roughly a 2-4 engineer-week effort; establishing and validating the currently
missing DX12/macOS platform paths can add further weeks. The first shader/atomic
gate should refine this range before a delivery commitment. Native Mac hardware
is required to close Metal validation; access was not established in this audit.

FSR adds compute and history memory. Native AA offers image quality, not a
resolution-based speedup. Lower render resolution may help GPU shading cost, but
does not directly fix chunk generation, meshing or CPU submission bottlenecks.

Only this assessment document was created. No engine settings, saves, sources,
dependencies or running game were changed, and no implementation success is claimed.
