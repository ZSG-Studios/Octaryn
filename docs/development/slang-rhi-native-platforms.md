# Native Slang RHI dependency setup

The Windows build remains `tools/build/slang-rhi.ps1`, with static D3D12/Vulkan,
the existing compiler settings and all registered dependency patches. Linux
and macOS now have native dependency acquisition/build commands. This is build
integration, not evidence of a Linux/macOS client build or FSR runtime pass.

The [pinned upstream CMake](https://github.com/shader-slang/slang-rhi/blob/e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc/CMakeLists.txt)
defines Vulkan for Linux and Metal for Darwin. The new command selects those
native backends explicitly, disables unrelated backends/tests/examples, and
builds the actual static `slang-rhi` target. It imports the same
[Slang 2026.17.1 release](https://github.com/shader-slang/slang/releases/tag/v2026.17.1).
All four Unix architecture archives have checked-in SHA256 digests obtained
from that release's asset metadata. Upstream's transitive dependency pins and
hashes remain unchanged.

Requirements: Python 3.12+, Git, CMake, Ninja, and a native C++17 compiler.
macOS additionally requires an installed Xcode SDK with Metal support. Linux
runtime execution requires a Vulkan loader and a working native GPU driver;
the dependency build itself fetches upstream's pinned Vulkan headers. Neither
the Linux driver nor an Apple SDK is supplied by this script.

The pinned RHI surface API accepts Xlib windows on Linux. The current SDL surface
bridge therefore requires X11 or XWayland (`SDL_VIDEO_DRIVER=x11`); native Wayland
surfaces are not implemented. This remains a platform qualification limitation,
alongside the untested native Linux build and driver execution.

Run on the destination machine:

```sh
python3 tools/build/slang-rhi.py --configuration Release --jobs 8
```

The script detects native x64/arm64, acquires the matching SDK into
`build/dependencies/slang-2026.17.1-{linux|macos}-{x64|arm64}`, verifies the
existing RHI checkout pin, applies exactly the PowerShell patch registry, and
builds into `build/dependencies/slang-rhi-{platform}-{arch}-Release`. Existing
SDKs/checkouts are validated rather than overwritten. A receipt is written
only after successful build. `--sdk-root` accepts an already installed SDK;
`--configuration Debug`, `RelWithDebInfo`, and `MinSizeRel` are supported.

Configure the client with matching `OCTARYN_TARGET_ARCH`, `CMAKE_BUILD_TYPE`,
and, if customized, `OCTARYN_SLANG_SDK_ROOT` / `OCTARYN_SLANG_RHI_BUILD_ROOT`.
The project's architecture default is x64, so native Apple Silicon requires
`-DOCTARYN_TARGET_ARCH=arm64`. Cross compilation and universal macOS binaries
are intentionally rejected by this dependency path.

`SlangRhiUnix.cmake` imports the static RHI/resources archives, Linux VMA,
Slang's shared compiler, and Threads/dl. Metal links Foundation, QuartzCore,
and Metal frameworks. It validates the build receipt and generated backend
config. Native and complete bundle recipes copy the SDK's Slang runtime
libraries including SONAME aliases; linked clients receive an adjacent-library
RPATH (`$ORIGIN` or `@loader_path`). There is no GFX linkage or raw graphics
implementation added here. Packaged loader resolution and shader/FSR passes
still need actual testing on each target OS.

Non-mutating plan/source checks, also runnable on Windows:

```sh
python tools/build/slang-rhi.py --print-plan --platform linux --architecture x64
python tools/build/slang-rhi.py --print-plan --platform macos --architecture arm64
python tools/validation/test_slang_rhi_bootstrap.py
```

The planning tests check all four native SDK/backend/architecture combinations,
custom SDK paths with spaces, configuration isolation, and rejected unsupported
targets. They do not execute CMake, the compiler, or a GPU.
