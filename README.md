# Octaryn

Octaryn is an experimental voxel game platform with a native C/C++ core, C# game
modules, and a playable creative sandbox. The desktop client renders a streamed
block world while a separate local server owns movement, block edits, world items,
and saves. Development is active; the current release baseline is Windows x64.

[Download releases](https://github.com/ZSG-Studios/Octaryn/releases) ·
[Documentation](https://zsg-studios.github.io/Octaryn/) ·
[Current integration notes](docs/development/repair-progress.md)

## What you can do

- Explore deterministic terrain with landforms, caves, water and lava; walk,
  sprint, jump, fly, and build with the creative block catalog.
- Set render distance up to **32 chunks outward (1,024 blocks)**. Columns are
  32 × 32 blocks, so radius 32 covers 65 × 65 columns, or a 2,080 × 2,080-block
  square including the center column. Terrain retains full voxel geometry with
  **no LOD**; loading and performance depend on the selected distance and hardware.
- Use a 50-slot pixel-art inventory, ten-slot hotbar, searchable creative catalog,
  cursor stacks, drag/drop, splitting, merging, sorting, and tooltips.
- Toss items into the world and pick them back up. The server handles item motion,
  collisions, merging and pickup grants; persistence and ordered acknowledgements
  protect counts across retries and restarts.
- Adjust display, render distance, lighting and temporal image quality from the
  RmlUi menus. The world continues simulating while menus are open.

The catalog supplies unlimited creative blocks. Crafting, armor, accessories,
survival progression and consumable block placement are incomplete. Natural tree,
bush and flower generation is not yet connected to the active terrain generator.

## Rendering and graphics APIs

All active first-party rendering uses **Slang shaders and standalone Slang RHI**:
terrain compute and indirect draws, sky, G-buffer/HDR scene lighting, forward
fluids, clouds, animated player skinning, selection, world items and the RmlUi
renderer. SDL3 handles windowing and input.

| API | Default target | Current qualification |
| --- | --- | --- |
| Direct3D 12 (DX12) | Windows | Packaged Windows x64 execution verified, including terrain, UI, items and FSR. |
| Vulkan | Linux; optional on Windows | Packaged Windows x64 execution verified, including Vulkan core/synchronization validation. Fedora 44/WSL2 software llvmpipe passes a relocated run; hardware Vulkan remains unqualified. |
| Metal | macOS | Slang emits Metal shader source and the platform path exists. Native macOS builds and GPU execution remain unqualified. |

The recorded GPU qualification uses an **AMD Radeon RX 9070 XT on Windows x64**.
It does not establish compatibility or performance for every GPU. Shader
compilation for a target is separate from running the application on that platform.
There is no active OpenGL, Direct3D 11, SDL GPU or Slang GFX renderer. Ray tracing
is unfinished. Internal HDR scene rendering currently presents SDR output;
HDR monitor output is not qualified.

**AMD FSR 2.2.1** is integrated through Slang/RHI with documented Godot reference
adaptations. Settings include Off, Native AA, Quality, Balanced, Performance,
Ultra Performance, custom render scale, RCAS sharpening and GPU-timed dynamic
resolution. UI stays at native display resolution. FSR 2 reconstructs frames;
it does not generate additional frames. Dynamic resolution targets a GPU budget
and cannot guarantee a frame rate when CPU work or fixed GPU costs dominate.

Backend selection is available before launch in PowerShell:

```powershell
$env:OCTARYN_CLIENT_GRAPHICS_API = 'vulkan' # or 'dx12' on Windows
.\tools\build\windows.ps1 -Action run-client
Remove-Item Env:OCTARYN_CLIENT_GRAPHICS_API
```

See [pipeline integration](docs/development/pipeline-parity.md),
[FSR integration](docs/development/fsr2-integration.md),
[FSR settings](docs/development/fsr-player-settings.md), and
[measured presentation performance](docs/development/presentation-performance.md).

## Technology and architecture

| Component | Technology and responsibility |
| --- | --- |
| Native core | C17/C++23; explicit client, server, shared and basegame ownership. |
| Managed modules | C# / .NET 10; game contracts, content/module registration and host bridges. Native and managed components both remain part of the build. |
| GPU abstraction | Standalone `shader-slang/slang-rhi`, pinned to `e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc`, with repository-maintained dependency patches. |
| Shader compiler | Slang 2026.17.1; SPIR-V, DXIL and Metal source targets. |
| Window and input | SDL 3.4.4. |
| User interface | RmlUi 6.2, RML/RCSS documents, original pixel assets, custom Slang RHI rendering. |
| Physics | Jolt 5.3.0 for authoritative player movement/collision. |
| Jobs and allocation | Native job ownership with Taskflow 4.0.0 and mimalloc 3.3.1. |
| Diagnostics | Frame CSVs, GPU timestamps, renderer readbacks and Tracy 0.13.1 instrumentation. |
| Build | CMake, Ninja, clang-cl on native Windows, and the .NET SDK. |

Terrain reconstruction is deterministic on the client and server. Generated seed
blocks remain transient; saves retain authoritative overrides and metadata rather
than entire generated chunks. Bounded background work and exact cave-noise caching
reduce repeated generation, while compute meshing, culling and capability-gated
indirect batching preserve the visible surfaces.

The playable session currently uses a supervised local server and bounded
process-file communication. **Internet multiplayer and remote-avatar replication
are not integrated.** The server executable is part of the local session; shared
networking contracts do not make this a public multiplayer server release.

| Directory | Owner |
| --- | --- |
| `octaryn-client/` | Presentation, input, GPU rendering, UI and client host. |
| `octaryn-server/` | Authority, simulation, validation, persistence and server host. |
| `octaryn-shared/` | Contracts, IDs, commands, snapshots and module/API policy. |
| `octaryn-basegame/` | Bundled game rules, content, assets and module implementation. |
| `cmake/` | Build policy, owner targets, dependencies and platform toolchains. |
| `tools/` | Build, packaging, validation, profiling and developer operations. |
| `docs/` | Architecture, integration reports and documentation source. |

## Run the Windows release

Download and extract the complete Windows x64 archive from
[Slang RHI Preview](docs/releases/2026-09-14-slang-rhi-preview.md). Install the
[.NET 10 Runtime for Windows x64](https://dotnet.microsoft.com/en-us/download/dotnet/10.0)
and the [Visual C++ x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
if they are not already installed. Keep the package's folders and libraries together.

Run `Launch-Octaryn.cmd` for DX12 or `Launch-Vulkan.cmd` for Vulkan. The client
starts and supervises its local server automatically. Use Save & quit or close
the client window to shut down the session cleanly. See the release notes for
world compatibility, package contents and qualification limits.

## Build and run on Windows

The maintained native entrypoint is `tools/build/windows.ps1`. Development needs
Visual Studio C++ Build Tools and Windows SDK, LLVM with `clang-cl`, CMake 3.28 or
newer, Ninja, Git, Python 3, and the .NET 10 SDK (`global.json` starts at 10.0.104
with feature-band roll-forward). The Windows x64 SDK from the official
[Slang 2026.17.1 release](https://github.com/shader-slang/slang/releases/tag/v2026.17.1)
must be extracted to
`build/dependencies/slang-2026.17.1` before building the pinned RHI dependency.

From the repository root in PowerShell:

```powershell
# First configure: prepare the required standalone rendering dependency.
.\tools\build\slang-rhi.ps1
.\tools\build\windows.ps1 -Action configure

# Build the client, server and their bundled managed/native dependencies.
.\tools\build\windows.ps1 -Action build
.\tools\build\windows.ps1 -Action run-client
```

For an already configured tree, `-Action build -Target octaryn_client_bundle`
rebuilds the client bundle. The default preset is `release-windows`, architecture
`x64`; output is under `build/release-windows/`. The client bundle is
`build/release-windows/client/bundle/Octaryn.Client.exe` and launches its own
local server. Keep the complete bundle together when moving it.

These are the maintained build commands; fresh-machine installation and other
platform presets still need independent qualification. CMake configuration can
fetch additional pinned dependencies. The dependency scripts and
[Slang RHI migration report](docs/development/slang-rhi-migration.md) describe
required inputs and the applied patches.

## Linux development

The native Linux release bundle builds on Fedora 44 under WSL2. See the [native build guide](docs/build/README.md)
for `tools/build/linux.py`, Slang RHI setup and platform prerequisites. The current
Vulkan surface path requires X11/XWayland. A relocated Linux run passes on
software llvmpipe; hardware Vulkan remains
unqualified. This experimental Fedora 44 binary requires glibc 2.43+ and
GLIBCXX_3.4.35. macOS/Metal execution remains separately unqualified.

## Controls and saves

| Action | Binding |
| --- | --- |
| Move / look | WASD / mouse; click the world to capture the mouse. |
| Jump / sprint | Space / Left Ctrl. |
| Toggle flight | F or F5. |
| Ascend / descend in flight | Space / Q or Left Shift. |
| Break / place / pick block | Left / right / middle mouse button. |
| Select hotbar | 1–0 or mouse wheel. |
| Inventory / creative catalog | I or E / B. |
| Toss one / toss stack | T / Ctrl+T. |
| Settings / close menu | Escape. |
| Zoom / HUD / fullscreen | Z / F3 / F11. |

In the source checkout, the default world is `saves/open-world-v2`; set
`OCTARYN_CLIENT_WORLD_PATH` to an absolute path to select another world.
A relocated bundle uses the platform's Octaryn application-data directory for
saves and logs. Save & quit or closing the window requests server shutdown and
final persistence. Terrain generator revision 2 requires compatible world
metadata; unversioned or incompatible saves are rejected to protect their edits.

## Development status and evidence

Fresh Windows x64 checks of the cleaned bundle passed **600 frames on DX12 and
Vulkan** at radius 4 (81 columns), with Native AA at 1280×720. A separate DX12
radius-32 run passed with 4,225 columns and 15,751,869 quads. All three runs exited
cleanly with no reported warnings/errors; their captures were inspected. This is
rendering/UI qualification, not an FPS benchmark or sustained-travel guarantee.

The native Linux release bundle and static/CPU checks pass on Fedora 44 under
WSL2. A relocated run passed 600 Native AA frames with 81 columns on **software
llvmpipe**. Hardware Vulkan remains unqualified: Dozen lacks the required
`VK_KHR_external_memory_capabilities` instance extension. This experimental
Fedora 44 binary requires glibc 2.43+ and GLIBCXX_3.4.35. Windows and Linux
static/CPU aggregates pass; all Windows GPU constituent targets also passed.
See the [platform matrix](docs/validation/build-matrix.md) for the exact scope.

The maintained build uses only the active owners and current native dependencies.
Obsolete GFX probes, duplicate source archives, container/UI launch tooling and
unused graphics/editor libraries were removed. FreeType now builds directly from
the same pinned source revision, without SDL_ttf or SDL_image. Column delivery
uses bounded asynchronous GPU mesh jobs; frame waits, CPU loading hitches and
sustained player travel still need further performance work.

- [Presentation, inventory and world-item qualification](docs/development/presentation-integration.md)
- [Terrain generator and save compatibility](docs/development/terrain-generation.md)
- [Exact terrain streaming cache and measured limits](docs/development/terrain-streaming-cache.md)
- [FSR integration fixes and moving-stream delivery](docs/development/fsr-streaming.md)
- [Fluid simulation integration and remaining runtime proof](docs/development/fluid-simulation-recovery.md)
- [Feature parity and known gaps](docs/development/feature-parity.md)
- [Repair progress and historical evidence](docs/development/repair-progress.md)

The [current architecture](docs/architecture/current.md), [build guide](docs/build/README.md)
and [validation guide](docs/validation/README.md) describe the maintained systems.
