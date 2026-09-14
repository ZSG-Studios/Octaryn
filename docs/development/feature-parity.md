# Capabilities and remaining gaps

This is the current integration map. Detailed reports retain the exact tested
configurations; a source implementation or a passing build alone is not runtime
qualification.

| System | Integrated behavior | Remaining limits |
| --- | --- | --- |
| Graphics | Standalone Slang RHI; Windows DX12 and Vulkan packaged execution, Slang shader graph, texture atlas, sky, scene HDR, clouds, fluids, player, items and UI. | Fedora 44/WSL2 software llvmpipe passes a relocated run; Linux hardware Vulkan, macOS/Metal, HDR display output and ray tracing remain unqualified. |
| Temporal presentation | Pinned FSR 2.2.1, Native AA, presets/custom scale, sharpening, GPU-timed resolution. | No frame generation; GPU budgets cannot resolve CPU bottlenecks. |
| Terrain | Deterministic revision-2 seed terrain, caves, materials, exact cache, full-detail GPU meshing and no LOD. | Natural vegetation stage is not connected; no Minecraft/Pumpkin seed parity claim. |
| Streaming | Bounded work, retained resources and indirect batching; selectable radius 32 at 32 blocks per column. | Initial GPU meshing can hitch; sustained moving-center performance needs further qualification. |
| Player | Local server-authoritative Jolt movement, source-time presentation, persistence and local skinned model. | Remote-avatar replication is not integrated. |
| Blocks and saves | Server validation, authoritative overrides, metadata and save/version checks. | Incompatible/unversioned terrain saves are rejected rather than silently migrated. |
| Inventory and items | 50-slot creative inventory, hotbar, cursor/drag/drop, server-owned tosses/pickups and durable acknowledgements. | Unlimited catalog supply; crafting, armor, survival loot and consumable placement are incomplete. |
| Fluids | Native evaluator, bounded scheduling and authoritative apply/save/publication. | Broader combined live behavior and sustained large-region workloads need qualification. |
| Communication | Supervised local server, bounded process-file I/O, stale-input handling and interpolation. | Internet/LAN transport is not integrated. |
| Modules | Managed contracts, manifest validation, packaged module activation and native host bridges. | API remains in development; host internals are not public module APIs. |

## Supporting evidence

- [Presentation integration](presentation-integration.md)
- [Terrain and save compatibility](terrain-generation.md)
- [Terrain performance](terrain-streaming-cache.md)
- [World item transactions](world-items.md)
- [Fluid authority](fluid-simulation-recovery.md)
- [Local-session recovery constraints](networking-recovery.md)
- [Source-parity reference map](presentation-restoration.md)
- [Current package qualification](release-packaging.md)

The original presentation source remains reference material for targeted parity
repairs. Obsolete migration roadmaps and archived baseline absence claims are no
longer part of the active documentation.
