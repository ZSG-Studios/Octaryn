# Octaryn — Lighting Preview (2026-09-14)

Windows x64 preview combining the active renderer, integrated voxel lighting,
RmlUi gameplay interface, authoritative local server and native build cleanup.
It follows the [Slang RHI Preview](2026-09-14-slang-rhi-preview.md), preserving
that release and its separate experimental Linux package.

Tag: `lighting-preview-20260914` (prerelease).

[Download this release](https://github.com/ZSG-Studios/Octaryn/releases/tag/lighting-preview-20260914).

## Packages and startup

- Game: `octaryn-lighting-preview-windows-x64-20260914.zip`.
- Native relink/source companion: `octaryn-lighting-preview-relink-windows-x64-20260914.zip`.
- SHA-256 checksums accompany the archives; package manifests identify the source
  commit and payload hashes. The relink companion supports replacing the static
  OpenAL library and is not needed to play.

Extract the complete game archive and keep its folders together. Install the
[.NET 10 Windows x64 Runtime](https://dotnet.microsoft.com/en-us/download/dotnet/10.0)
and [Visual C++ x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
Run `Launch-Octaryn.cmd` for DX12 or `Launch-Vulkan.cmd` for Vulkan. The client
starts and supervises its authoritative local server. Save & quit or closing the
client requests clean server shutdown and final persistence.

## What changed

- **Shared voxel ray tracing.** Streamed columns reuse the exact GPU surface
  geometry and material atlas for procedural acceleration structures. Bounded
  builds/refits, retained snapshots and scene-change notifications feed all
  lighting effects through standalone Slang RHI.
- **RT sun shadows and DDGI.** Hardware ray queries produce sun visibility with
  temporal filtering. A scrolling 2,048-probe DDGI volume supplies diffuse
  indirect illumination, with bounded updates, history rejection and reduced
  ambient beyond valid coverage.
- **ReSTIR local lighting.** Point, spot and rectangle lights use temporal and
  spatial reservoir reuse, with selected-light RT visibility on supported high
  tiers. This is Octaryn's Slang implementation; it does not require RTXDI.
- **Raster shadow modes.** Low/medium settings use three directional shadow
  clipmaps and tiled local lighting. A cached six-face map shadows one selected
  point/spot light. Unsupported RT devices use raster paths; DDGI requires
  usable acceleration structures, inline queries and bindless resources.
- **Integrated presentation.** Deferred terrain lighting composes before the
  existing fluids, player/items, clouds, FSR 2.2.1, tone mapping and RmlUi.
  Lighting options and diagnostic views expose quality and coverage controls.
- **Build and simulation cleanup.** Active owner systems replace disconnected
  GFX probes, container/bootstrap paths and unused dependency fetches. Native
  FreeType replaces the SDL_ttf wrapper. Fluid topology notifications now wake
  nearby slope-lookahead donors so blocked outlets can redirect water/lava.

See the [lighting architecture and evidence](../development/lighting-architecture.md)
for resource budgets, capability gates, scheduling, source ownership and diagnostics.

## Verification scope

The integrated cleanup/lighting build passed `octaryn_all`,
`octaryn_validate_static` and `octaryn_validate_cpu`, including 35,529 native
fluid checks. Its packaged client SHA-256 was
`4c6c259305a7c6b60c8af2aa9aaf905f54d765d263f893167f719e3b569b2bec`.
Windows RX 9070 XT DX12 high and Vulkan low qualifications each passed 600 frames
with no native graphics warnings/errors. The architecture report retains the
exact result files and separate earlier 1440p, resize, RT visibility, probe,
selected raster-shadow and inspected water/sky capture evidence.

These records describe their tested builds; final archive hashes and source
identity are supplied by the release manifests and checksums. A successful build
or bounded capture does not establish every scene, GPU or sustained-travel case.

## Known limits and compatibility

- Current integrated lighting is qualified on Windows AMD hardware only. There
  is no new Linux package in this release. The previous Fedora 44 software
  llvmpipe result does not qualify this lighting build, Linux hardware Vulkan,
  macOS/Metal, NVIDIA or Intel execution.
- Forward player/items, clouds and transparent surfaces are outside the new
  deferred receiver/dynamic-instance path. Full moving-object shadows/GI parity
  and a general reflection system are incomplete.
- DDGI has one volume and no cascades; thin-wall leaking and temporal response
  delay remain possible. Raster sun coverage is finite. Local fallback shadows
  only one point/spot light; other lights and rectangles remain unshadowed.
- Large heterogeneous light populations need further testing. Reservoir and
  history memory costs are substantial; no universal FPS or VRAM guarantee is
  implied by the bounded lighting fixture.
- Render distance remains selectable up to 32 chunks outward (1,024 blocks),
  with no LOD. The previous preview's radius-32 test does not qualify the new
  lighting at that distance; sustained travel and radius 128 are unqualified.
- This remains a creative local sandbox. Internet multiplayer, survival
  progression and full natural vegetation generation are incomplete.
- Terrain generator revision 2 and authoritative edit-only saves remain active.
  Incompatible or unversioned saves are rejected. Keep backups of existing
  worlds before trying a preview; relocated packages use Octaryn application-data
  storage for saves and logs.

The [README](../../README.md) covers controls and technology;
[feature status](../development/feature-parity.md) records broader gameplay gaps.
