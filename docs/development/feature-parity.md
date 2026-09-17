# Capabilities and remaining gaps

This is the current integration map. Detailed reports retain the exact tested
configurations; a source implementation or a passing build alone is not runtime
qualification. Tagged preview package results predate the current source lighting
integration; its Linux software and radius-32 results do not qualify newer lighting.

| System | Integrated behavior | Remaining limits |
| --- | --- | --- |
| Graphics | Standalone Slang RHI; Windows DX12 and Vulkan packaged execution, Slang shader graph, texture atlas, sky, scene HDR, clouds, fluids, player, items and UI. | Preview-baseline Fedora 44/WSL2 software llvmpipe run passed; Linux hardware Vulkan, macOS/Metal and HDR display output remain unqualified. |
| Lighting | Shared voxel AS, RT sun shadows, coarse/fine DDGI, resident torch/lava lights through deterministic tiled direct lighting, and raster fallbacks. Animated player casts full-body ray/raster shadows in first and third person. Probe histories survive scene refresh; edited AS columns remain visible until replacement. | Finite probe coverage and temporal response; player/items are not deferred GI receivers and world items do not cast dynamic shadows; other platform/vendor execution and lighting at radius 32 remain unqualified. |
| Temporal presentation | Pinned FSR 2.2.1, Native AA, presets/custom scale, sharpening, GPU-timed resolution. | No frame generation; GPU budgets cannot resolve CPU bottlenecks. |
| Terrain | One built-in natural generator, revision 3, with shared server/client trees, bushes and flowers, including neighboring canopies. Normal startup uses open-world-v3. Exact cache, full-detail GPU meshing and no LOD remain. | Older, flat and empty worlds are rejected and preserved on disk. No Minecraft/Pumpkin seed parity claim; ore veins, aquifers and exposed overhangs remain incomplete. |
| Streaming | Bounded work, retained resources and indirect batching; selectable radius 32 at 32 blocks per column. | Initial GPU meshing can hitch; sustained moving-center performance needs further qualification. |
| Player | Local server-authoritative Jolt movement, source-time presentation, persistence and local skinned model. | Remote-avatar replication is not integrated. |
| Blocks and saves | Server validation, authoritative overrides, metadata and save/version checks. | Incompatible/unversioned terrain saves are rejected rather than silently migrated. |
| Inventory and items | 50-slot creative inventory, hotbar, cursor/drag/drop, server-owned tosses/pickups and durable acknowledgements. | Unlimited catalog supply; crafting, armor, survival loot and consumable placement are incomplete. |
| Fluids | Native evaluator, bounded scheduling and authoritative apply/save/publication. | Broader combined live behavior and sustained large-region workloads need qualification. |
| Communication | Supervised local server plus a single-peer LiteNetLib/LES dedicated session; Windows loopback pose, chunk, item and reconnect qualification. Arch stores remote session state. | Internet/LAN impairment, multiple authoritative players and LES input prediction remain unqualified; see [remote-session repair](remote-session-repair.md). |
| Modules | Managed contracts, manifest validation, packaged module activation and native host bridges. | API remains in development; host internals are not public module APIs. |

## Supporting evidence

- [Presentation integration](presentation-integration.md)
- [Integrated lighting and Windows GPU evidence](lighting-architecture.md)
- [DDGI, block lights and vegetation repair evidence](voxel-lighting-vegetation-repair.md)
- [Lighting movement, digging stability and player shadows](lighting-motion-repair.md)
- [Terrain and save compatibility](terrain-generation.md)
- [Cave lighting, moving shadows and single-generator repair](cave-lighting-generator-repair.md)
- [Terrain performance](terrain-streaming-cache.md)
- [World item transactions](world-items.md)
- [Fluid authority](fluid-simulation-recovery.md)
- [Local-session recovery constraints](networking-recovery.md)
- [Source-parity reference map](presentation-restoration.md)
- [Current package qualification](release-packaging.md)

The original presentation source remains reference material for targeted parity
repairs. Obsolete migration roadmaps and archived baseline absence claims are no
longer part of the active documentation.
