# Current implementation status

The working application uses standalone Slang RHI with Slang shaders, SDL3 and
RmlUi. Windows defaults to DX12; Vulkan also has Windows GPU qualification.
The client starts an authoritative local server and an interactive world.

## Integrated systems

- Exact terrain reconstruction and compute meshing, bounded column streaming,
  culling and indirect batching without LOD.
- Textured materials, sky/day-night, HDR scene lighting, fluids, clouds, local
  player skinning, selection and native-resolution RmlUi menus.
- Counted creative inventory with durable server-owned world-item transactions.
- FSR 2.2.1 modes, sharpening, custom scale and GPU-timed dynamic resolution.
- Shared voxel acceleration structures, RT sun shadows, DDGI lighting,
  tiled local direct lighting and raster fallbacks; see the [lighting report](lighting-architecture.md).
- The 2026-09-18 user direction restores DDGI allocations, updates, settings and
  debug views and removes the experimental replacement; see
  [restoration status and evidence](ddgi-restoration.md).
- Local Jolt movement, player/world persistence and bounded session I/O.

## Evidence and remaining work

| Area | Focused report |
| --- | --- |
| Presentation, UI and items | [Integration and GPU qualification](presentation-integration.md) |
| FSR image/input fixes and streaming | [FSR streaming](fsr-streaming.md) |
| Player quality controls | [FSR settings](fsr-player-settings.md) |
| Exact terrain caching | [Terrain cache](terrain-streaming-cache.md) |
| Meshing/frame overlap | [Voxel throughput](voxel-throughput.md) |
| Latest cloud/water correction | [Cloud/water jitter](cloud-water-jitter.md) |
| Restored DDGI passes, settings and GPU qualification | [DDGI restoration](ddgi-restoration.md) |
| Live DDGI updates, source changes and adaptive scheduling | [DDGI response repair](ddgi-live-response.md) |
| DDGI flicker and temporal accumulation | [DDGI stability repair](ddgi-stability.md) |
| DDGI source-update reverb and cave-leak investigation | [DDGI reverb investigation](ddgi-reverb.md) |
| DDGI light-update latency and idle budget | [DDGI light-update repair](ddgi-light-update-repair.md) |
| GI range, visibility and menu integration | [Lighting range integration](lighting-range-integration.md) |
| Background renderer initialization and corrected frame pacing | [Startup responsiveness](startup-responsiveness.md) |
| Fluid authority | [Fluid simulation](fluid-simulation-recovery.md) |
| Current release | [Package workflow](release-packaging.md) |

Historical benchmark numbers belong to their recorded build, backend, hardware
and workload. Loading hitches and sustained moving-center streaming remain
separate performance work. The pre-RT preview passed a relocated Fedora 44/WSL2 software llvmpipe run; Linux hardware
Vulkan and macOS/Metal still need qualification. The [feature status](feature-parity.md) records gameplay gaps.

Use the current [build guide](../build/README.md) and
[validation guide](../validation/README.md). Removed migration plans and obsolete
bootstrap reports remain available in Git history, not as active instructions.
