# Standalone Slang RHI integration

The active renderer links the real `shader-slang/slang-rhi` interfaces, pinned to
`e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc`, with Slang 2026.17.1. First-party
shaders and submissions use Slang and RHI throughout the frame graph. The obsolete
GFX renderer and its unported probes are not part of the maintained system.

`WorldRendererDevice.cpp` selects the native backend: DX12 on Windows, Vulkan on
Linux, Metal on macOS. `OCTARYN_CLIENT_GRAPHICS_API` explicitly selects the API.
DX12/Vulkan are runtime-qualified on the recorded Windows/Radeon configuration;
the preview baseline passed a relocated Fedora 44/WSL2 software llvmpipe run; Linux hardware Vulkan
and macOS/Metal execution remain unqualified.

Current source additionally integrates RT sun shadows, DDGI, ReSTIR and raster
fallbacks, with separate [Windows AMD DX12/Vulkan evidence](lighting-architecture.md).
The tagged preview predates this lighting work; its Linux software and radius-32
results do not establish the newer lighting's platform or large-world qualification.

Windows dependency preparation uses `tools/build/slang-rhi.ps1`; native Unix uses
`tools/build/slang-rhi.py`. The shared patch registry preserves pinned descriptor,
sampler and indirect draw capability corrections. Receipts validate source/SDK,
architecture and configuration before CMake imports the dependency. Bundles carry
the runtime libraries and shader source required by the native client.

## Current pipeline and evidence

- [Integrated pipeline and image qualification](pipeline-parity.md)
- [RT, DDGI, ReSTIR and shadow fallback integration](lighting-architecture.md)
- [Presentation, items and FSR](presentation-integration.md)
- [FSR shader/SDK integration](fsr2-integration.md)
- [Native platform dependency setup](slang-rhi-native-platforms.md)
- [Native builds](../build/README.md)
- [Validation groups](../validation/README.md)

Compilation proves interface/toolchain compatibility. GPU checks must execute the
selected backend and inspect both actual captures and diagnostics. Windows tests
or emitted Metal source do not establish cross-platform runtime support.
