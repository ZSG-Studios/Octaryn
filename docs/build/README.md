# Native builds

Build the current owner targets directly. Source and dependency outputs are
partitioned under `build/<preset>/` and `build/dependencies/`. The client bundle
contains the server and managed/native payloads needed for a local session.

## Windows x64

Install Visual Studio C++ Build Tools/Windows SDK, LLVM clang-cl, Git,
Python 3 and the .NET 10 SDK. CMake and Ninja are provisioned automatically
(pinned releases under `build/dependencies/tools/`), as is the Slang
2026.17.1 Windows x64 SDK (downloaded by the `rhi` action into
`build/dependencies/slang-2026.17.1`).

Windows tool checklist:

- Visual Studio C++ Build Tools with the Windows SDK (the entrypoint imports
  the VS developer environment automatically).
- LLVM `clang-cl` on the VS/LLVM tool path, plus Git.
- Pinned CMake 3.30.5 and Ninja 1.12.1 are downloaded automatically on first
  use; set `OCTARYN_NO_TOOL_PROVISION=1` to require them from PATH instead.
- Python 3 with Pillow (`pip install pillow`) for notice/atlas checks.
- .NET 10 SDK (`global.json` pins 10.0.104 with feature-band roll-forward).
- Slang 2026.17.1 Windows x64 SDK is acquired automatically by the `rhi`
  action; a manual extract to `build/dependencies/slang-2026.17.1` (or
  `--sdk-root`) remains the offline fallback.
- GitHub CLI (`gh`) for the prior-release attribution download used by
  `package`; without it notice collection refuses to run.
- To run a packaged build: .NET 10 x64 Runtime and Visual C++ x64
  Redistributable.

```powershell
python tools/build/windows.py --action rhi --preset release-windows
python tools/build/windows.py --action configure --preset release-windows
python tools/build/windows.py --action build --preset release-windows
python tools/build/windows.py --action run-client --preset release-windows
```

The Windows entrypoint is `tools/build/windows.py`, the Python twin of
`tools/build/linux.py`, and both cover the full flow: `rhi` bootstraps the
standalone Slang RHI dependency, `package` runs the release pipeline.
`tools/build/slang-rhi.py` remains directly runnable (including
`--print-plan` inspection); the entrypoints delegate to it. The remaining
`tools/build/support/*.py` helpers are invoked by CMake during configure and
bundling.

The default is `release-windows`, x64. The maintained client target is
`octaryn_client_bundle`; the complete build is `octaryn_all`. The client is
`build/release-windows/client/bundle/Octaryn.Client.exe`.

The cleaned Windows build and static checks pass. Fresh 600-frame Native AA
rendering/UI runs pass on DX12 and Vulkan at radius 4; DX12 also passes radius 32
(4,225 columns). All three exit cleanly with no reported warnings/errors, and
captures were inspected. Windows and Linux static and CPU validation aggregates both pass.
See the [platform matrix](../validation/build-matrix.md) for scope and evidence.

## Linux

The native Linux release bundle builds on Fedora 44 under WSL2. Use native Clang,
CMake/Ninja, Python 3.12+, Git and .NET 10, with the platform development libraries
needed by SDL3 and the Vulkan/X11 surface. The project needs a C++23-capable
compiler (the RHI dependency alone uses C++17), CMake 3.28+ and a recent Ninja
with `compdb-targets` for source validation. Install X11 development libraries
for SDL3 and headers for enabled audio backends. FreeType is source-built from
the pinned SDL fork commit `9973564cfa63763a3e4ac67c09147899539b1e07`; SDL_ttf
and SDL_image are not build prerequisites.

Linux tool checklist (the entrypoint aborts naming the first missing tool):

- `clang`/`clang++` with C++23 support, CMake 3.28+, a recent Ninja, Git and
  Python 3.12+ with Pillow.
- .NET 10 SDK (`global.json` pins 10.0.104 with feature-band roll-forward).
- X11 development libraries (`xorg-dev` covers the SDL3 X11 surface needs),
  Vulkan headers (`libvulkan-dev`), `libudev-dev`, and audio backend headers
  (`libasound2-dev`, `libpulse-dev`, `libpipewire-0.3-dev`).
- The Slang SDK is acquired automatically by the `rhi` action into
  `build/dependencies` (same as Windows).
- To run a packaged build: .NET 10 runtime, a Vulkan loader/driver with the
  X11/XWayland surface path (`SDL_VIDEO_DRIVER=x11`), and compatible system
  shared libraries.
- From Windows PowerShell the same `tools/build/linux.py` commands work
  directly: they auto-detect WSL2 and re-run inside the default distribution
  (override with `--wsl-distro` or `OCTARYN_WSL_DISTRO`), which must have the
  tools above installed. Native Linux behavior is unchanged. Pass argument
  paths with forward slashes; backslashes are dropped by WSL argument
  forwarding. The pinned RHI dependency is built
through the entrypoint:

```sh
python3 tools/build/linux.py --action rhi --preset release-linux --jobs 8
```

The [native platform dependency guide](../development/slang-rhi-native-platforms.md)
records SDK paths, target architecture and required libraries. The active surface
bridge uses Xlib; choose X11/XWayland (`SDL_VIDEO_DRIVER=x11`). An environment that
can compile the client is not necessarily able to present Vulkan graphics.
Use the native entrypoint after preparing dependencies:

```sh
python3 tools/build/linux.py --action configure --preset release-linux
python3 tools/build/linux.py --action build --preset release-linux --jobs 8
python3 tools/build/linux.py --action run-client --preset release-linux
```

These commands built the current Linux release bundle. Static/CPU aggregates
and 35,529 native fluid checks pass. A relocated 600-frame Native AA run passes
on software llvmpipe under Fedora 44/WSL2, with no warnings/errors and an inspected
capture. Hardware Vulkan remains unqualified: Dozen lacks the required
`VK_KHR_external_memory_capabilities` instance extension. This experimental
Fedora 44 binary requires glibc 2.43+ and GLIBCXX_3.4.35, verified from ELF symbols.

## macOS

The native dependency script supports Metal and x64/arm64, with an installed Xcode
SDK. It does not establish a qualified native client package or GPU run.

## Packages and validation

Keep all bundle assets/libraries together. Windows packages currently require
the .NET 10 x64 runtime and Visual C++ x64 redistributable. The client supervises
its local server automatically. See [packaging](../development/release-packaging.md)
and [validation](../validation/README.md).

[Linux package tooling and notice prerequisites](linux-packaging.md) documents the
native tar.gz packager and the remaining platform-specific attribution work.
