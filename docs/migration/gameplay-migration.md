# Gameplay Migration

Basegame owns high-level rules and content. It does not own chunk storage, mesh data, persistence execution, transport, lighting propagation, or direct native services.

Gameplay execution is scheduler-owned. Basegame systems declare high-level logic and data access, then the host coordinator schedules eligible work on the worker pool. Basegame must not create raw threads, private task schedulers, timers, or custom worker pools.

C/C++ host owners may drive managed ECS/gameplay through explicit owner bridge APIs. Those bridges stay outside module-facing contracts; modules use shared scheduling, command, query, registry, and capability APIs only.

## Current Managed Surface

- `octaryn-basegame/Source/Managed/GameContext.cs` owns the current managed basegame context.
- `octaryn-basegame/Source/Module/BasegameModuleRegistration.cs` declares the bundled module manifest.
- `docs/migration/gameplay-migration-map.md` tracks old native gameplay surfaces and the shared/server contracts they need.

## Contract Targets

| Old behavior | Basegame responsibility | Host/server responsibility |
| --- | --- | --- |
| player movement intent | Declare high-level movement rules and inputs | Simulate authority and client prediction |
| block interaction | Declare interaction rules | Validate and apply block edits server-side |
| block selection/inventory | Declare high-level inventory rules | Provide snapshots and accepted commands |
| player persistence | Declare save-facing gameplay state shape | Own save format, load/store, and compatibility checks |
| world raycast/query | Request host query contracts | Execute query in client/server owner |

## Next Port Tasks

- Keep client bundle/module payload reads rooted at the packaged client bundle. Native client probes should prove both `Data/` and `Assets/` payloads without falling back to workspace paths.
- Replace atlas metadata-only checks with real atlas texture upload and material sampling once the native rendering path moves out of validation rectangles.
- Keep shared scheduling contracts enforced for game/module systems: tick phase, read/write access, ordering dependencies, cancellation, and result handoff.
- Keep `GameContext.Tick` routed through the host coordinator and expand runtime/profiling validation before moving heavy gameplay systems.
- Replace map entries with real gameplay systems as shared command/query contracts are added.
- Keep direct native services out of basegame. Add small host APIs when a rule needs host data.
