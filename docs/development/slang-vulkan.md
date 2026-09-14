# Slang and Vulkan baseline

Current backend direction (2026-09-13): the active open world has been ported to
standalone `shader-slang/slang-rhi`, with Vulkan selected through its API.
See [slang-rhi-migration.md](slang-rhi-migration.md) for current build/runtime
evidence. GFX-specific validation below is historical; it does not qualify the
new backend. Remaining original presentation features resume on standalone RHI.

Verified on Windows x64, 2026-09-13.

The renderer uses Slang GFX (`slang-gfx.h`, `gfx::IDevice`), not standalone
slang-rhi. All 11 device creation sites explicitly select Vulkan and request
Slang's direct SPIR-V target. GFX's default device choice could select D3D12
on Windows, so leaving that choice implicit did not satisfy Vulkan ownership.

## SDK

The current upstream stable SDK is Slang 2026.17.1, published 2026-09-11.
Its Windows x64 package includes the existing GFX API and successfully compiles
and runs the engine's occupancy compute path.

Installed location: `build/dependencies/slang-2026.17.1`.
Configure override: `-DOCTARYN_SLANG_SDK_ROOT=<complete SDK root>`.
`SLANG_SDK_ROOT` initializes the cache variable when supplied as an environment
variable. SDK discovery requires `include/slang-gfx.h`, `lib/gfx`, and
`bin/slangc`; missing dependencies are fatal instead of creating an unavailable
renderer. Windows packaging consumes `OCTARYN_CLIENT_SLANG_RUNTIME_FILES`:
`gfx.dll`, `slang.dll`, `slang-compiler.dll`, and `slang-rt.dll`.

Package: `slang-2026.17.1-windows-x86_64.zip`.
Verified SHA-256:
`a41896cba523ecfecaf67d18bdb7767349112a45540ac0b88e8ce48e10ce239f`.
The release digest matches the downloaded file; `slangc -version` reports
`2026.17.1`. The GFX DLL imports the Slang compiler and Windows system DLLs.
The direct SPIR-V path does not stage GLSL/glslang or LLVM compiler plugins.

## Source and runtime validation

The first-party shader inventory contains 26 `.slang` files under
`octaryn-client/Shaders`. No authored HLSL/GLSL files or embedded alternate
shader source was found in client, server, shared, or basegame source.
All program creation uses `ShaderModuleSourceType::SlangSourceFile`.
The common compute-program builder reports compiler diagnostics on failure.

`tools/validation/validate_client_slang_shaders.py` compiles every one of the
26 files into a Slang IR module, including files that only declare helper
functions/types. It additionally compiles 12 compute/vertex/fragment entry
points to SPIR-V using `-emit-spirv-directly`. All 26 module and 12 entry-point
compilations passed. Each stage has a distinct output path; the prior raster
vertex/fragment outputs incorrectly overwrote the same validation file.

The existing `ClientVoxelOccupancyProbe.cpp` was compiled against the production
backend and payload code using clang-cl and the SDK. Vulkan device creation,
compute dispatch, fence completion, and readback passed with occupancy counts
empty=0, uniform=32768, mixed=24576. This is actual GPU execution, not only
shader compilation.

The same probe passed when launched from `C:/Windows` with executable-relative
`Client/Shaders` and the four staged DLLs. `SlangShaderPath` resolves this bundle
layout through the existing asset path API. When a bundle shader root exists,
its files are authoritative; missing files fail. Unbundled developer probes
can use the repository-relative Slang tree from their configured working
directory. Alternate shader languages are never substituted.

These checks establish a Windows Slang/Vulkan compute and shader-loading
baseline. Aggregate graphical client launch, sustained frame performance,
and Linux/macOS runtime behavior require their own evidence.

## Native swapchain probe

The existing swapchain probe now selects the SDL Windows HWND through
`SDL_PROP_WINDOW_WIN32_HWND_POINTER` and `gfx::WindowHandle::FromHwnd`.
The Linux X11 handle path remains present, and Cocoa uses the SDK's NSWindow
handle. The probe API and handle result field use platform-neutral names.

Previously it acquired and immediately presented an uninitialized image.
The probe now transitions its newly acquired image from Undefined to
RenderTarget, clears it, transitions it to Present, submits that command buffer,
and then presents. Queue completion precedes release of GPU resources and the
Vulkan surface; SDL window destruction occurs afterward on every path.

The existing `ClientRenderBackendSwapchainProbe.cpp` was compiled with production
backend code, the aggregate build's SDL3 static library, and Slang 2026.17.1.
It ran successfully on Windows: `driver=windows`, `acquired=1`, `transitioned=1`,
`submitted=1`, `presented=1`, `idle=1`, exit code 0. This is a real hidden-window
Vulkan surface/swapchain submission and presentation check.
GFX's Vulkan `present()` currently returns success without propagating
`vkQueuePresentKHR` results, which limits the strength of its success result.

The bootstrap now reports failure if its swapchain checks fail.

## GPU dependency repair and validation layer

The first aggregate GPU runs exposed four real failures: greedy counts were
nondeterministic, prefix totals were zero, and quad emission/indirect generation
then failed their unchanged expected results. The Slang shaders themselves
compiled and their independently checked fixture expectations were correct.

The cause was GFX's Vulkan translation of `ResourceState::ShaderResource`:
`calcAccessFlags` selects input-attachment access and `calcPipelineStageFlags`
selects only the fragment stage. Those scopes do not cover a compute shader's
structured-buffer reads. Producer-to-consumer buffer barriers now use
`ResourceState::General`, giving memory-read/write visibility across all command
stages. The repair applies to the four probe chains and the live raster pass
graph; shader algorithms and expected counts were not changed.

Khronos validation 1.4.357 was recovered by copying the preserved layer DLL,
JSON manifest, and spirv-val executable into
`build/dependencies/vulkan-validation`. Copied-file hashes match the backup;
the backup was not modified. Process-local `VK_LAYER_PATH`,
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, and `VK_LAYER_SETTINGS_PATH`
enable it with Core and Synchronization checks. Loader diagnostics identify the
AMD Radeon RX 9070 XT, and each recorded run confirms the layer is enabled.

Synchronization validation also found that GFX waits on swapchain acquisition
at BOTTOM_OF_PIPE, which did not order the initial image transition. An explicit
execution dependency now bridges that wait before the transition, using a
zero-resource GFX barrier from Present to General. This adds no CPU wait or
extra queue submission. The swapchain probe then passed with zero validation
errors or warnings.

All nine rebuilt aggregate probes now pass with zero Core/Synchronization
validation errors or warnings: frame resources, payload upload, occupancy, face
masks, greedy counts, prefix ranges, packed-quad emission, indirect generation,
and native swapchain. Their binary timestamps postdate the relevant source
changes. No `runtime_unavailable` result was accepted.

Unchanged exact results include greedy counts `[0, 6, 98304, 130]`, material
sums `[0, 252, 36372480, 27950]`, prefix offsets `[0, 0, 6, 98310]`, total
98440 quads, and three nonempty indirect draws covering 98440 instances.
Logs, exits, timestamps, and counts are recorded in
`logs/client/gpu-probes/fixed-results.json` and neighboring per-probe logs.

## Packaged raster validation

Running the actual packaged raster path under the same validation settings
exposed errors that the nine resource/compute/swapchain probes did not cover.
The vertex shader used D3D-style instance/vertex semantics, which Slang lowers
to the Vulkan index minus its base offset. That both required an unenabled
DrawParameters feature and discarded the indirect command's `firstInstance`
offset when indexing the shared packed-quad buffer.

The raster shader now uses `SV_VulkanInstanceID` and `SV_VulkanVertexID`.
The former preserves the packed-quad range offset; the latter has identical
corner indexing for the existing zero `vertexOffset` commands. Compiled SPIR-V
contains raw InstanceIndex/VertexIndex, no BaseInstance/BaseVertex subtraction,
and no DrawParameters capability. `spirv-val --target-env vulkan1.2` passes.
The raster session requests the stable `spirv_1_3` target explicitly.

The other error was a readback copy from a color-attachment image layout.
GFX's `readTextureResource` copies from the supplied state without changing
layouts. The render path now transitions RenderTarget to CopySource after
drawing, reads back in CopySource state, and transitions back before rendering
the next retained frame. Reused buffer barriers likewise use the General
state established by the pass graph.

After rebuilding and restaging, the actual packaged `Octaryn.Client.exe` was
launched from `C:/Windows` with Core and Synchronization validation enabled.
All three retained frames and two stream updates completed with zero validation
errors or warnings, three indirect draws, 36 instances, and successful color
readback. The corrected packed-range indexing produces 24 nonclear pixels in
this fixture, compared with 12 before the semantic repair. The Windows
swapchain checks also passed, and the process exited 0.

Source and bundled raster shader SHA-256 both match
`5aceb19ce1b34023ffbccf8ff151a0f6450120a92b3a9db0f661d309535cdcd0`.
Evidence is in `logs/client/packaged-validation-fixed-results.json` and its
neighboring stdout, stderr, and app logs. This packaged run explicitly uses the
four-chunk fallback fixture and reports `radius32_stream_available=0`; it does
not prove a visible full-radius world or sustained gameplay performance.

## Interactive terrain presentation and image proof

The visible client now owns a retained Vulkan renderer through `WorldRenderer.h`.
It accepts real `WorldStream` columns spanning Y=-256 through 255, counts exposed
faces on the GPU, allocates exact face storage, then emits unit quads and indirect
draw arguments on the GPU. Rendering uses signed world positions, the existing
camera owner's 90-degree vertical field of view, perspective projection and depth
testing. Columns outside the requested center/radius are retired. The verified
radius is 2: 25 full-depth columns. This is separate from the preserved four-chunk
diagnostic APIs and is not radius-32 residency qualification.

Actual image inspection found defects despite successful draws and zero Vulkan
validation messages. GFX shallow-copies texture descriptors and dereferences
`optimalClearValue` when a framebuffer is created later. The initial depth clear
pointer referenced an expired resize stack frame, leaving the displayed image
black. Color/depth clear values now live with the retained renderer. The vertex
shader also inverted clip Y even though GFX already sets a negative Vulkan
viewport height; that duplicate inversion was removed.

Rendering now targets a retained color texture and copies that exact image to
the swapchain. The swapchain has transfer-destination usage but lacks
transfer-source usage, so this path also permits valid diagnostic readback.
`OCTARYN_CLIENT_CAPTURE_PATH` requests a BMP after the complete requested column
neighborhood is resident. It records the camera and nonclear pixel count and
writes adjacent `.quads.bin` GPU face data for geometry inspection. Capture is
off during normal execution. Layout transitions cover rendering, copy, readback
and reuse; capture does not substitute a different image for presentation.

The final packaged run completed 300 frames with Core and Synchronization
validation enabled, zero errors/warnings, 25 columns and 1,039,331 GPU quads.
Recorded retained face/argument/color/depth bytes were 24,002,496; that figure
excludes driver, swapchain and descriptor allocations. The actual 1280x720 image
contains terrain and sky, with 856,810 nonclear pixels. Its camera was
(0,35.62,0), yaw 0, pitch -0.35 and vertical FOV 1.570796 radians. Evidence:
`logs/client/open-world-fixed-validation.log`, `open-world-fixed.bmp`, and
`open-world-fixed.bmp.quads.bin`. Independent parsing of all 1,039,331 captured
GPU faces found no duplicate records within a column, out-of-column positions,
invalid face directions or air materials; emitted Y ranged from -256 to 84.
Earlier narrow-FOV captures filled the whole
image with terrain; independent authoritative-terrain ray checks confirmed that
view was physically possible and did not establish a further renderer defect.

This is a working initial terrain presentation, not complete visual parity.
Block colors and fixed face tint remain primitive presentation, without the
original texture atlas or material system. Quads are exposed unit faces rather
than greedy rectangles; adjacent resident columns still emit hidden boundary
faces. The renderer waits for its submitted frame before reusing resources;
frame overlap, high-distance performance, complete atlas integration and full
radius-32 geometric coverage remain unqualified. First-party shader compilation
now covers 28 modules and 15 entry points, all authored in Slang.

## Upstream references

- [Slang 2026.17.1 release](https://github.com/shader-slang/slang/releases/tag/v2026.17.1)
- [Slang GFX getting started](https://shader-slang.org/slang/gfx-user-guide/01-getting-started.html)
- [SPIR-V target documentation](https://docs.shader-slang.org/en/stable/external/slang/docs/user-guide/a2-01-spirv-target-specific.html)
- [Compiler options](https://github.com/shader-slang/slang/blob/v2026.17.1/docs/user-guide/08-compiling.md)
- [SDL native window properties](https://wiki.libsdl.org/SDL3/SDL_GetWindowProperties)
- [GFX Vulkan swapchain implementation](https://github.com/shader-slang/slang/blob/v2026.17.1/tools/gfx/vulkan/vk-swap-chain.cpp)
- [GFX Vulkan resource command encoding](https://github.com/shader-slang/slang/blob/v2026.17.1/tools/gfx/vulkan/vk-command-encoder.cpp)
- [GFX Vulkan state/access translation](https://github.com/shader-slang/slang/blob/v2026.17.1/tools/gfx/vulkan/vk-helper-functions.cpp)
- [GFX Vulkan queue submission](https://github.com/shader-slang/slang/blob/v2026.17.1/tools/gfx/vulkan/vk-command-queue.cpp)
- [GFX retained texture descriptor](https://github.com/shader-slang/slang/blob/v2026.17.1/tools/gfx/renderer-shared.h)
- [GFX framebuffer clear-value consumption](https://github.com/shader-slang/slang/blob/v2026.17.1/tools/gfx/vulkan/vk-framebuffer.cpp)
