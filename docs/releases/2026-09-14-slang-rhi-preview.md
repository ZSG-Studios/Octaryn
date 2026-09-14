# Octaryn — Slang RHI Preview (2026-09-14)

Native Windows x64 and experimental Fedora 44 Linux x64 preview of the restored
Octaryn client and local authoritative server. This follows
**Old Architecture Shareable Builds 2026-04-30**
(`old-architecture-shareable-20260430`), which offered historical Linux-native and
Windows/Proton snapshots. This release packages the active owner systems and
their current rendering and gameplay work; it is an in-development preview.

Tag: `slang-rhi-preview-20260914` (prerelease).

Game archives:

- `octaryn-slang-rhi-preview-windows-x64-20260914.zip`
- `octaryn-slang-rhi-preview-linux-x64-fedora44-20260914.tar.gz`

## What changed

- **Standalone Slang RHI rendering.** First-party GPU passes use Slang shaders
  through `shader-slang/slang-rhi`. Windows defaults to DirectX 12, with Vulkan
  available explicitly. This replaces the restored Slang GFX integration.
- **Restored world presentation.** Textured voxel materials, animated atlases,
  sky, clouds, HDR processing, fluids, selection feedback and a local skinned
  player share the active render pipeline. Texture filtering and greedy-mesh
  seam repairs preserve crisp nearby materials and continuous voxel surfaces.
- **FSR 2.2.1.** Temporal reconstruction, Native AA, quality presets, custom
  render scale, sharpening and GPU-timed dynamic resolution are exposed in
  settings. FSR is off by default; this implementation does not generate frames.
- **RmlUi menus and inventory.** The original pixel art is presented in a
  Terraria-inspired 50-slot inventory with a ten-slot hotbar, searchable creative
  catalog, cursor stacks, drag/drop and right-click stack handling. Item tosses
  and pickups use server authority and durable count acknowledgements. Menu
  pointer movement no longer accidentally activates buttons or settings.
- **Octaryn terrain revision 2.** Shared native generation supplies climate-based
  terrain, sea-level water and enclosed three-dimensional caves to both client
  reconstruction and server queries. Only authoritative changes and metadata
  persist; generated seed blocks remain transient.
- **Full-detail voxel performance work.** Exact GPU greedy meshing, bounded
  streaming work, retained scratch/resources, frame overlap and capability-gated
  draw batching reduce rendering and streaming overhead without LOD. The
  selectable maximum is 32 chunks: 1,024 blocks outward at 32 blocks per chunk.
- **Moving-stream delivery and FSR integration fixes.** New column meshes use
  bounded asynchronous count/emit jobs; query data and visible geometry publish
  together when ready. The FSR opaque snapshot includes players and world items,
  preventing opaque surfaces from being marked as transparent contributions.
- **Fluid updates in every direction.** Blocking an outlet now wakes the water
  and lava cells whose slope decisions depended on it, allowing outward flow to
  resume. Direct falling and neighboring updates retain priority within bounded
  queues. Regression coverage includes all four directions, diagonals, drainage,
  queue saturation and authoritative edit persistence.
- **Current build and source layout.** Removed disconnected GFX renderer code,
  obsolete probes, old container/UI launchers, unused dependency builds and
  duplicate reference archives. Native Windows and Linux commands share the
  active CMake owners; static, CPU and GPU checks have distinct targets.
- **Native Linux repairs.** The pinned Slang bootstrap handles the SDK's
  versioned libraries and native Clang build. .NET hosting discovers the installed
  runtime through nethost, and delivered native libraries use relative loader
  paths. Atlas animation files accept normal CRLF line endings; X11 presentation
  selects a supported linear RGBA/BGRA format for compute output.

## Graphics APIs and platform status

| Platform / API | Status for this preview |
| --- | --- |
| Windows x64 / DirectX 12 | Native default; packaged development GPU checks recorded on Windows with Radeon RX 9070 XT. |
| Windows x64 / Vulkan | Explicit alternative; packaged development GPU checks recorded on the same Windows hardware. |
| Fedora 44 Linux x64 / Vulkan | Native build and CPU checks pass. Relocated client/server, Native AA and UI execution verified through WSL2/XWayland using Mesa llvmpipe software rendering. Hardware-accelerated Linux execution is not qualified; WSL's Dozen driver is unsupported by this pinned RHI configuration. |
| macOS / Metal | Native dependency/build integration and Metal shader emission exist; native client and FSR execution are not qualified. |

Choose Vulkan with `OCTARYN_CLIENT_GRAPHICS_API=vulkan`; choose DirectX 12 with
`OCTARYN_CLIENT_GRAPHICS_API=dx12`. API selection is explicit: availability of a
backend in the source does not imply runtime support on every GPU or OS.

The implementation combines native C/C++, managed C#/.NET 10 host/module systems,
SDL3 window/input handling, RmlUi, Jolt collision/physics and Taskflow jobs.
Standalone Slang RHI owns graphics submission. Ray tracing remains unfinished.

## Installation and world compatibility

Extract the entire Windows package into a writable directory and keep its client,
server, assets and native libraries together. The current managed bundle is
framework-dependent and requires the Windows x64 [**.NET 10 runtime**](https://dotnet.microsoft.com/en-us/download/dotnet/10.0)
and [Visual C++ x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
Run `Launch-Octaryn.cmd` for DX12 or `Launch-Vulkan.cmd` for Vulkan. Both launch
the client and its bundled local server automatically. A compatible
GPU and current driver are required; the recorded Radeon runs do not establish
minimum specifications or compatibility across other vendors.

The Linux archive requires **glibc 2.43**, a libstdc++ runtime providing
**GLIBCXX_3.4.35**, the .NET 10 x64 runtime, Vulkan loader/driver, and X11 or
XWayland. These ABI requirements were inspected in the built ELF files. Extract
with executable permissions preserved and run `./Launch-Octaryn.sh`. It is a
Fedora 44 build, not a universal Linux binary. WSL qualification explicitly used
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`; this selects software
rendering and does not establish playable GPU performance. The ordinary launcher
does not silently change drivers.

Use a new world directory for this preview. Natural worlds carry a generator
identity, revision, seed and mode; incompatible or unversioned existing saves are
rejected rather than silently rewritten. Keep old release saves separately.
Do not add revision-2 metadata manually to make an old world load.

## Known limits

- Client/server operation uses a local process and file exchange. Internet/LAN
  multiplayer transport and remote player-avatar replication are unfinished.
- The inventory remains creative building with unlimited catalog supply.
  Crafting, armor, accessories, survival loot and consumable placement are not
  complete Terraria-style gameplay.
- Natural trees, bushes and flowers are not wired into active generation.
  Terrain uses fixed seed 1337; configurable seeds, exposed cave entrances,
  aquifers and river flow are not implemented by revision 2.
- Frame waits, CPU work and moving-center streaming can still hitch. Short settled
  benchmarks do not establish sustained-travel performance. Earlier Vulkan
  batching measurements are not DX12 performance claims.
- Dynamic resolution cannot guarantee its requested frame rate when CPU or
  fixed GPU costs dominate. FSR 2 is reconstruction, not frame generation.
- The Linux hardware driver path, other distributions, macOS/Metal execution and
  cross-platform terrain reproducibility require separate qualification. This
  release does not provide Proton or macOS binaries.

## Validation evidence

Fresh relocated bundle runs use isolated worlds/settings, Native AA at 1280x720,
internal UI validation and actual captured graphics output. Windows DX12 and
Vulkan and Linux llvmpipe each completed 600 resident frames with all 81 requested
columns, 320,431 quads and zero graphics validation warnings/errors. Screenshots
were inspected. Additional Windows DX12 maximum-distance qualification completed
600 resident frames with all 4,225 columns; this is not a sustained-travel or
minimum-frame-rate guarantee.

Windows and Linux static/CPU validation pass, including **35,529 fluid checks**,
authoritative edits, managed publication and persistence. Windows graphics checks
also cover FSR backends, seven UI screens, descriptor allocation, raster culling,
player rendering, shader bundles and GPU readback. The current diagnostic harness
isolates settings and explicitly selects its test distance.

The packagers verify per-file hashes and archive contents. Companion relink
commands were executed successfully; the Windows relinked client also completed
an isolated DX12 runtime test. Archive SHA-256 values are supplied with the
download assets. Detailed methods and limits are in the development reports:

- `docs/development/fsr-player-settings.md`
- `docs/development/presentation-integration.md`
- `docs/development/terrain-generation.md`
- `docs/development/voxel-throughput.md`
- `docs/development/fsr-streaming.md`
- `docs/development/vegetation-recovery.md`
- `docs/development/fluid-simulation-recovery.md`

## Distribution materials

The game archive includes launchers, installation instructions, a per-file
SHA-256 manifest and third-party notices. The matching Windows ZIP and Linux
tar.gz relink companions include OpenAL Soft source, client object files,
non-system link libraries and relinking instructions.
Source is available at the release tag. [Faithful](https://faithfulpack.net/) and
[ClassicFaithful contributors](https://github.com/ClassicFaithful/Classic-32x-Jappa-Java)
provide the block texture-pack artwork; attribution and their license are included in
`THIRD_PARTY/ClassicFaithful/`. This is not an official Faithful release.
