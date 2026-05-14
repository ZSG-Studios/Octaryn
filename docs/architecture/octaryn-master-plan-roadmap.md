# Octaryn Master Plan: Roadmap

This file is part of the canonical `octaryn-master-plan.md` policy set. It owns the active port queue, invariants, lighting hold, near-term work, open decisions, and hard boundaries.

## Old-Architecture Port Gap Queue

The old architecture remains the source inventory for behavior parity, but only non-lighting slices may move before a dedicated lighting plan exists. Current validated coverage includes basegame block/content catalogs, basegame atlas payload metadata and client-side atlas tile sampling, basegame worldgen rules, server terrain generation and spawn-column seeding, server block edits/persistence overrides, world time persistence, client snapshot presentation, and graphical client launch probes that consume generated server terrain.

Remaining non-lighting gaps should be handled in the order below unless direct validation shows a blocker. This queue refines the near-term world, interaction, client/server launch, physics/presentation, and content-tooling phases; it does not reopen lighting work.

| Gap | Old source | Destination | Validation |
| --- | --- | --- | --- |
| Client bundle data and asset discovery | `references/old-architecture/source/core/asset_paths.*`, app startup asset reads | `octaryn-client/Source/App/AssetPaths/` and client launch probes | Client app launch probe must read module `Data/` and `Assets/` from the bundle root, not workspace paths. |
| Real native atlas upload/render path | `references/old-architecture/source/render/atlas/`, `references/old-architecture/source/render/world/`, shader atlas use | `octaryn-client/Source/Rendering/` and `octaryn-client/Shaders/` | Client launch validation now samples bundled color-atlas tiles and validation-only normal/specular material swatches through SDL texture rendering; full SDL_GPU 2D array upload, shader binding, mips, animation updates, and PBR validation remain open. |
| Player movement, collision, spawn, and camera state | `references/old-architecture/source/app/player/`, `references/old-architecture/source/core/camera/camera.*`, `references/old-architecture/source/physics/` | Basegame movement rules, server physics authority, client prediction/presentation | Focused player/physics probes plus client presentation checks; no client-owned authority. |
| Block raycast and interaction UX | `references/old-architecture/source/app/overlay/interaction.*`, `references/old-architecture/source/world/edit/` | Shared command/query contracts, basegame interaction rules, server block authority, client input routing | Interaction probe should cover raycast target selection, accepted/rejected edits, support rules, and snapshot replication. |
| Player, chunk, and world-save metadata | `references/old-architecture/source/core/persistence/player.cpp`, `chunk_files.cpp`, `world.cpp`, `world_save.cpp`, `world_snapshot.cpp` | `octaryn-server/Source/Persistence/` with shared save/version contracts only | Player save file round trips, current JSON chunk-column override files, and active world-save metadata summaries are server-owned coverage; compressed chunk cache semantics, save migration/replay probes, corruption handling, and compatibility IDs remain open. |
| Dynamic fluids and gases | `references/old-architecture/source/world/edit/water.cpp`, old material/block behavior, and future fluid rules | Basegame fluid/gas declarations and server active-region simulation | Fluid/gas probes must prove deterministic cross-chunk queues, tick budgets, snapshots, and client presentation handoff. |
| Items, inventories, recipes, tags, loot, and product UI | old gameplay/content and UI surfaces | `octaryn-basegame/Source/Content/`, `Gameplay/`, `Ui/` with client retained-UI execution | Content collision validators, inventory/action probes, UI declaration/focus/raycast probes. |

## Performance Invariants

Do not invent exact frame/server budgets before hardware and player-count targets are chosen. Commit to invariants first:

- Hot ECS iteration must stay allocation-free on the hot path.
- Server authority, world edits, replication packing, and save writes must be phase-ordered and profiler-visible.
- Chunk meshing, save writes, asset cooking, and other bulk work must be backgroundable through the host scheduler.
- Fluid and gas simulation must be active-region and budgeted.
- Client presentation, server authority, module execution, and tool jobs must have separate Tracy/profiling ownership.
- Validation and probes should measure concrete costs before a system is scaled up.

## DDGI And Lighting Hold

DDGI is the intended future lighting direction, but it is not active work until the user provides a dedicated lighting plan.

Do not port old CPU skylight propagation as the lighting implementation path.

Do not add DDGI, lighting probes, server lighting contracts, or client lighting rewrites from this ECS/API plan. Lighting files in `references/old-architecture/` are reference material only until that plan exists.

## Near-Term Architecture Work

1. Boundary freeze: ratify owner rules, capability taxonomy, trust tiers, world constants, dependency decisions, and no-generic-bucket rules in docs and validators.
2. Blank owner structure and migration maps: keep active roots clean and record source-to-destination or removal reasons for every old-architecture file touched.
3. API rename and contract cleanup: remove `Octaryn.Engine.Api` dependencies, unsafe shared bridges, and basegame host-only references.
4. Shared descriptor spine: add declaration contracts and generator-ready descriptors for components, systems, entities, blocks, items, UI, input, game state, commands, snapshots, queries, replication, persistence, manifests, compatibility, API exposure, and package allowlists.
5. Host scheduling spine: define main/coordinator/worker thread contracts, scheduled system declarations, read/write sets, barriers, cancellation, and thread-safety validation.
6. CMake/build spine: keep shared build policy, owner targets, dependency aliases, platform modules, toolchains, presets, target names, output layout, logs, and Podman builder paths separated.
7. Runtime spine: add native owner skeletons for ECS storage, Octaryn scheduler policy over Taskflow, LiteNetLib/LiteEntitySystem-backed networking, and first save-container contracts only when concrete validation accompanies them.
8. World and interaction spine: port authoritative blocks, entities, inventories, chunk snapshots, block edits, fluids/gases, and voxel interaction kernels into server/client/basegame owners.
9. Client/server launch spine: package `client_server_app` with version-matched `server/`, add readiness contract, keep dedicated server separate, and validate bundle ownership.
10. Physics and presentation spine: add Jolt-backed host physics wrappers, retained UI, Yoga layout integration, SDL3_ttf text path, input/focus/raycast routing, and world-space UI probes.
11. Cooked content and tooling spine: add asset cooking, shader/material ABI checks, package manifests, basegame product UI, content registry collision checks, and asset hash validation.
12. External mod hardening: add IL sandbox scans, trust/signature policy, artifact identity binding, multiplayer compatibility negotiation, save migration replay, and module hash checks before scaling mod support.
13. Scale-up and polish: add profiling thresholds, focused benchmarks, admin/editor tooling, and optional Recast/Detour navigation planning after the core runtime spine is stable.

## Open Decisions To Revisit Deliberately

- First shipping Linux/Windows hardware targets.
- Authoritative server tick rate and intended player counts.
- Save compatibility promise: strict backward compatibility, migration windows, or best-effort.
- External native code policy: never, first-party only, or signed trusted extensions only.
- Product UI authoring preference: C# fluent/declarative only, or optional markup layer.
- World model scope: single shard, dimensions, or both.
- Navigation/AI scope and whether Recast/Detour becomes first-wave or deferred.
- Runtime audio consolidation after benchmarks.

## Hard No List

- No raw ECS world handles to modules.
- No client/server implementation assembly access from modules.
- No renderer, socket, filesystem, process, thread, native pointer, scheduler, or backend resource access. Capabilities may grant bounded Octaryn API handles, never raw internals.
- No raw physics world or transport session access.
- No direct third-party backend types in shared or module public APIs.
- No broad "mod context" with everything attached.
- No basegame-only shortcuts that future mods cannot use.
- No old `Engine` namespace or compatibility wrapper.
- No generic runtime/common/helpers/misc buckets.
