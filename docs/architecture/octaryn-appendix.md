# Octaryn Appendix

> **For agentic workers:** Use the maximum available agents/subagents where available. Inspect first, make a brief source-to-destination plan, then execute. This file defines structure and boundaries only.

`docs/architecture/octaryn-master-plan.md` is the canonical Octaryn master plan. This file is an appendix and migration checklist. If this file conflicts with the master plan, update this file to match the master plan.

**Goal:** Same end goal: port Octaryn into strict client, server, basegame, shared, and focused native support layers with a native C/C++ core first, no general `engine` folder, and no behavior rewrite. C# ECS/gameplay and client/server networking are intentional tools where they fit best. The platform should load game modules through explicit APIs and reject incompatible modules before they run.

**Architecture:** `old-architecture/` is source material only, never an active implementation target. Client owns presentation and rendering, server owns authority and persistence, basegame is the first bundled game module with high-level mechanics, content, and assets through API contracts, and shared owns contracts, value types, module manifests, compatibility rules, and validation-facing APIs. Existing focused support libraries should stay as build/internal libs under the owner that uses them instead of becoming a new generic runtime root.

**Tech Stack:** C++23/C17, CMake, SDL3 GPU/Vulkan, .NET 10, C# latest, Arch ECS for managed gameplay/module ECS, native owner ECS/storage for high-throughput host paths, LiteNetLib and LiteEntitySystem as hidden client/server networking backends behind Octaryn contracts, Taskflow under Octaryn-owned scheduling policy, Tracy, targeted runtime/profiling validation. RenderDoc is an external developer tool, not a workspace-managed dependency.

---

## Current State

- The active repository root is `/home/zacharyr/octaryn-workspace`.
- The old `octaryn-engine/` tree is deleted from the working tree and preserved as tracked `old-architecture/` source material.
- `octaryn-client/`, `octaryn-server/`, `octaryn-shared/`, and `octaryn-basegame/` have real owner project files.
- `octaryn-client/` owns the managed native ABI export edge through `ClientHostExports` and a client-owned `BasegameModuleActivator`; `octaryn-server/` owns the server host export edge, server module activation, and server-side module validation.
- `octaryn-basegame/` contains the current managed game context and basegame module registration. The old gameplay migration map now lives under `docs/migration/` so active basegame source stays focused on content and gameplay implementation.
- `octaryn-shared/` now contains timing/input-only host-frame contracts, a narrow request command contract, module manifests, dependency/content/asset/compatibility declaration records, exposed host API IDs, module capability IDs, module runtime/build package allowlists, framework API group allowlists, sandbox denied-group IDs, and manifest validation for duplicate, blank, unexposed API, unapproved capability, unapproved package, unapproved framework API group, and malformed declaration requests.
- Root MSBuild policy rejects unknown project owners, package references in `octaryn-shared`, host-only packages outside client/server, unapproved direct module packages, analyzer packages with runtime assets, unapproved resolved runtime/analyzer packages for module owners, and unclassified packages in module `project.assets.json`.
- Active `cmake/` has a concrete new-architecture scaffold: root `CMakeLists.txt`, root `CMakePresets.json`, owner CMake modules, dependency policy placeholders, platform modules, toolchain files, and new `tools/build` wrappers. It builds managed owner targets, native owner aggregates, hostfxr bridge facades, launch probes, bundles, debug tool payloads, and validation targets without porting the old monolith.
- Root debug tooling is first-class under `tools/`: `tools/ui/` owns the PySide workspace control app, `tools/profiling/` owns Tracy build/launch/capture, and `tools/build/` owns build orchestration, Linux host setup, Podman builder files, bootstrap entrypoints, sysroot setup, and shared shell helpers.
- `docs/` is the top-level informational/documentation-only folder. It is not a source, build, module, or runtime owner.
- `old-architecture/.octaryn-cache/` may contain ignored generated/reference cache files. Do not treat cache content as tracked source material or a migration source unless it is explicitly promoted.
- `old-architecture/tools/build/layout.sh` still points native builds at `old-architecture/`; active root `tools/build/` is reserved for intentionally ported new-architecture build helpers.
- Reference material lives under `refrances/`, including Minecraft 26.1.2, Iris, and Complementary Reimagined.
- DDGI, skylight propagation, lighting architecture, and the old CPU skylight implementation are on hold until the user provides a dedicated lighting plan. The current basegame `skylightOpacity` catalog value is content metadata only; do not add server lighting contracts, DDGI code, lighting probes, or client lighting rewrites before that plan exists.
- The next active port slice should stay non-lighting: continue basegame content/rules, server authority/persistence, client presentation that does not alter lighting, module validation, build ownership, or tool cleanup.
- The ECS/API direction is captured in `docs/architecture/octaryn-master-plan.md`: blocks, items, entities, UI state, input actions, game state, fluids, gases, and world interactions are ECS-backed declarations and systems; C++ hosts own fast backend execution, networking, persistence, scheduling, and validation behind explicit APIs.
- The dependency and subsystem direction from `/home/zacharyr/Downloads/deep-research-report.md` is finalized as planning guidance with project corrections that Arch ECS, LiteNetLib, and LiteEntitySystem remain in use: Arch ECS for managed gameplay/module ECS, native owner ECS/storage for high-throughput host paths, Jolt-first hidden physics, custom retained UI with hidden Yoga layout, LiteNetLib/LiteEntitySystem-backed networking behind Octaryn contracts, custom binary saves, Glaze JSON metadata, LZ4 hot compression, Zstd cold compression, and no public all-in-one runtime stack.
- The core host baseline is a flying camera with no default player physics and flat blank terrain. Target worlds are 512 blocks tall and centered vertically; existing 256-height or chunk-edge-derived height constants are migration debt, not the destination model.
- Singleplayer must still run through server authority. `client_server_app` must carry the bundled `server/` payload, start that server when creating or loading a singleplayer world, wait for world setup/module validation/save initialization to complete, then connect through the same command/snapshot contracts used by multiplayer. The standalone server bundle remains a dedicated headless terminal/server executable path.
- Product UI, including the main menu, pause menu, inventory screens, HUD, world-space panels, nameplates, block/entity panels, and game-specific options, belongs to `octaryn-basegame` or another active game module through explicit UI contribution APIs. Core/client-owned UI is limited to debug, diagnostics, profiler, validation, editor/developer, and emergency host surfaces. The client renderer owns screen-space rendering, render-to-texture, textured world-space quads/panels, focus, and raycast input routing.
- Developer-facing math, geometry, deterministic random, time, diagnostics, serialization, networking, and physics tools may be exposed only as Octaryn-owned API contracts and capability-gated handles. Do not expose raw physics worlds, transport sessions, renderer handles, native pointers, third-party backend types, sockets, filesystems, schedulers, or raw ECS storage to basegame, game modules, or mods.

## Phase 0 Blockers

These are current transitional violations and hard blockers. Do not add or expand module-facing behavior that depends on them. Work touching these areas must remove the blocker, add real enforcement, or keep the affected code non-activated.

- Keep `octaryn-basegame` on `octaryn-shared` contracts and do not reintroduce a reference to `old-architecture/source/api/Octaryn.Engine.Api.csproj`.
- Keep unmanaged managed-host exports in host-owned code such as `octaryn-client`, not `octaryn-basegame`.
- Keep `AllowUnsafeBlocks` out of `octaryn-basegame`. Module code must not keep unsafe/native bridge access as a normal permission.
- Keep unsafe native function-pointer bridges out of `octaryn-shared`; shared exposes safe module contracts such as manifests, module frame contexts, module command request facades, declarations, and capability handles. Raw host frame/command ABI types are owner/internal only.
- Keep host-only package references out of `octaryn-basegame`; `LiteNetLib` and `LiteEntitySystem` belong only in client/server transport projects when transport is wired.
- Keep LiteEntitySystem host-owned and hidden behind Octaryn networking contracts. It is part of the intended client/server networking stack, but not a module-facing API.
- Keep `Octaryn.Client.csproj`, `Octaryn.Server.csproj`, and `Octaryn.Shared.csproj` as real SDK project definitions with owner-routed outputs.
- Keep pre-load manifest validation file-backed: module content declarations must point at existing `Data/` records, asset declarations must point at existing `Assets/` or `Shaders/` files, and undeclared content/assets must fail validation.
- Replace runtime `legacy*` content schema fields with stable Octaryn IDs or generator-only migration metadata before treating basegame catalogs as final module data.
- Keep resolved transitive package validation enforced for basegame and extend the same runtime/build-analyzer allowlist model to external game modules and mods when those package projects are introduced.
- Keep source-level framework API allowlist enforcement and post-build binary metadata inspection active for namespaces/types/members. External binary-only modules still need artifact identity/package/content binding before they are trusted.
- Keep the owner thread contract enforced before porting heavy compute systems: one main thread, one coordinator thread, and a scalable worker pool with at least two workers. All computation and gameplay logic must be scheduled through this pool or through host APIs backed by this pool, with runtime/profiling validation expanded before heavy systems move.
- Keep owner-project validation that rejects host-only packages outside `octaryn-client` and `octaryn-server`.
- Expand the new CMake scaffold with native owner targets and targeted platform configure checks before claiming native platform/toolchain parity with the old monolith.
- C/C++ owner code may call managed host exports only through the resolved hostfxr owner bridge. Bridge readiness requires exact managed method-name resolution, ABI size/version validation, owner bundle discovery, failure-path validation, and direct runtime launch evidence.

## Hard Boundaries

- No new top-level `engine/` or `octaryn-engine/` folder.
- No new `Octaryn.Engine.*` namespaces.
- Do not port by copying the monolith shape into a new name.
- Do not rewrite behavior first; preserve behavior by moving it into the right owner.
- Do not make root `cmake/` a dumping ground. Shared build policy, owner targets, dependencies, platform detection, and toolchains must stay in separate named folders.
- Do not mix host platform logic with owner target definitions. Windows and Linux distro-family platform logic must be isolated behind platform/toolchain modules.
- Do not put networking packages in `octaryn-basegame`.
- Do not put GPU upload, mesh upload, render descriptors, windowing, audio, or UI in server.
- Do not put authoritative world edits, save ownership, or server simulation in client.
- Do not implement singleplayer as client-local authority. Singleplayer is `client_server_app` with client presentation connected to a bundled server-owned simulation, persistence, validation, replication, and physics path.
- Do not put product-specific game rules in shared or native support libraries.
- Do not put voxel host internals in `octaryn-basegame`: chunks, mesh data, lighting propagation, persistence, replication, transport, storage formats, or low-level world mutation belong to shared/client/server APIs and implementations.
- Do not load a game module by reaching into its internals. Game modules must declare their API version, required capabilities, content registrations, assets, dependencies, and compatibility constraints through shared contracts.
- Do not let client, server, or tools silently accept incompatible game modules. The host must validate manifests, API versions, dependency ranges, required capabilities, content IDs, asset declarations, and multiplayer compatibility before activation.
- Do not expose broad implementation assemblies to game modules. Module code may use only explicitly approved shared APIs, approved host interfaces, and approved .NET packages.
- Do not allow module-owned NuGet dependency drift. Game modules must compile against a deny-by-default package allowlist; unapproved .NET packages, framework namespaces, reflection, native interop, filesystem, process, threading, dynamic loading, and networking access are rejected unless the allowlist says otherwise.
- Do not let gameplay, module, client, server, or tool code create arbitrary computation threads. The host owns threading; systems submit work to the coordinator and worker pool through approved scheduling APIs.

## Destination Roots

```text
octaryn-client/
octaryn-server/
octaryn-basegame/
octaryn-shared/
tools/
cmake/
  Shared/
  Owners/
  Dependencies/
  Platforms/
    Windows/
    Linux/
  Toolchains/
    Windows/
    Linux/
docs/
refrances/
old-architecture/
build/<preset>/<owner>/
build/<preset>/deps/
build/dependencies/
logs/<owner>/
```

## Threading And Work Scheduling

The active architecture must use one explicit host-owned scheduling model. All systems that do meaningful computation or gameplay logic must be written so they can run safely through that model.

Thread roles:

- Main thread: owns process startup/shutdown, platform event pumping, presentation handoff, final frame submission, and the narrow places where a platform or graphics API requires main-thread access. It must not become the place where gameplay, chunk generation, simulation, asset processing, or other bulk computation runs.
- Coordinator thread: owns frame/tick scheduling, dependency graph assembly, work submission, synchronization fences, cancellation, and deterministic handoff between client, server, basegame/module logic, and tools. It schedules work; it does not run bulk work itself except for tiny coordination tasks.
- Worker pool: owns computation. It starts with a minimum of two worker threads and scales up to the available system cores according to host policy. The pool is the execution path for simulation systems, gameplay systems, world generation, mesh/data preparation, asset processing, validation jobs, async save/load preparation, replication preparation, and other CPU-heavy logic.

Scheduling rules:

- All computation systems and gameplay logic must run as jobs in the worker pool or through approved host APIs that schedule onto the pool.
- New code must be thread-safe by default: no hidden global mutable state, no unsynchronized shared containers, no lifetime borrowing across jobs without an explicit owner, and no blocking waits on the main thread.
- Systems must declare their read/write access, ordering dependencies, cancellation behavior, and frame/tick ownership before they enter the scheduler.
- Client presentation work may prepare data on workers, but graphics API calls, window events, and final presentation stay on client-owned main-thread/platform paths.
- Server authority work runs through coordinator-scheduled jobs and commits through deterministic server tick barriers. Save, validation, replication, and world-edit commits must have explicit synchronization points.
- Basegame and external modules do not own threads, tasks, or timers. They receive scheduled system/update entry points and capability handles from the host.
- Tools may use the same scheduler model for offline work, but tool-specific worker use must still be isolated under `tools/` or the owning module tool folder.

API and sandbox impact:

- `octaryn-shared` should expose scheduling contracts only as stable capability-shaped APIs: job scopes, tick phases, read/write declarations, cancellation tokens, and result handles. It must not expose native worker internals or third-party scheduler types.
- Client/server own scheduler implementations and thread creation. Shared/basegame/modules own only contracts and scheduled logic.
- Approved packages such as `Arch.System` may define gameplay systems, but those systems must be driven by host scheduling rather than direct task/thread creation from module code.
- Native C/C++ owner code may drive managed ECS/gameplay or networking through explicit owner bridges. Those bridges are host implementation details, not module APIs.
- Raw `System.Threading`, `Task.Run`, custom timers, unmanaged threads, and ad hoc thread pools remain denied for module code. Any exception requires a documented host API and an allowlist update.
- Native job support should use the approved `octaryn::deps::taskflow` wrapper through focused owner targets such as `octaryn_native_jobs`; do not hand-roll a parallel scheduler in client, server, or basegame.
- Taskflow executes Octaryn schedules; it does not define module scheduling policy. Octaryn owns phases, read/write declarations, barriers, cancellation, deterministic ordering, and profiler ownership.

Validation requirements:

- Source/API validation must reject raw threading and task scheduling from module code.
- Module manifests must declare scheduled systems with phase, owner, resource reads/writes, ordering, flags, and commit barrier before a host can activate scheduled work.
- Scheduler-facing systems must have targeted runtime/profiling validation through direct runs, Tracy captures, focused logs, or benchmarks. Do not use smoke tests or `ctest` as a substitute unless explicitly requested.
- CMake and MSBuild owner targets must keep scheduler support owner-partitioned under `build/<preset>/<owner>/` and `logs/<owner>/`.

## Client And Server Launch Modes

Octaryn has two user-facing launch modes that share the same authority model.

- Graphical client: `octaryn_client_bundle` is the local playable application. It owns windowing, input, rendering, audio, client UI, local prediction, presentation, and the user flow for choosing singleplayer or multiplayer.
- Client server app: when the client creates or loads a singleplayer world, `client_server_app` starts a server-owned local session from the bundled `server/` payload. That session owns world creation/loading, module validation, basegame activation, server ticks, simulation, persistence, replication state, physics, and shutdown.
- Dedicated server: `octaryn_server_bundle` remains separately runnable as a headless terminal/server package. It owns the same authority path as the bundled server, but has no client rendering, audio, UI, windowing, or GPU dependencies.
- Multiplayer client: remote multiplayer uses the same shared command, snapshot, tick, replication, disconnect, and error contracts as singleplayer. Singleplayer must not use privileged client-only mutation paths.

Packaging and ownership rules:

- The client bundle may copy server-owned artifacts into `server/` for singleplayer, but copied artifacts do not change ownership. Server implementation remains in `octaryn-server/`, server build outputs remain under `build/<preset>/server/`, and client build outputs remain under `build/<preset>/client/`.
- Do not create a monolithic target that compiles client presentation and server authority into one owner. Bundle composition is allowed; ownership mixing is not.
- The bundled `server/` payload must be version-matched to the client, shared contracts, and selected game/basegame modules before activation.
- Basegame and modules are validated before the bundled server reports readiness. The client may show progress and errors, but validation and activation decisions stay server/shared-contract driven.
- Bundled server logs stay under `logs/server/`; graphical client logs stay under `logs/client/`, even when both are launched from the client.

Startup readiness contract:

1. Client selects create/load singleplayer world and requested game modules.
2. Client starts or attaches to the bundled server session through a client-owned launch/supervision API.
3. Server validates module manifests, compatibility, requested capabilities, content/assets, package allowlists, and multiplayer/singleplayer compatibility.
4. Server creates or opens the world save, initializes server world state, activates basegame/modules, and starts server ticks.
5. Server publishes a local endpoint/session handle and a ready snapshot.
6. Client connects through shared command/snapshot contracts, receives initial state, and enters world presentation.
7. Client return-to-menu, quit, crash, or world-close flows stop the bundled server through server-owned shutdown APIs and wait for save-close completion where required.

Planned validation:

- Keep dedicated server launch probes separate from `client_server_app` launch probes.
- Keep the `client_server_app` readiness probe focused on the bundled server app startup path and expand it toward ready snapshot publication, clean disconnect, and save-close behavior as those runtime contracts land.
- Add bundle validation that confirms the client bundle contains the required `server/` payload and that the dedicated server bundle contains no client presentation payload.

## Non-Lighting Port Queue

Use this queue when choosing the next old-architecture slice. The master plan remains authoritative; this list is a source-to-destination checklist for incomplete non-lighting behavior.

| Priority | Slice | Old source | Destination owners | Current status |
| --- | --- | --- | --- | --- |
| 1 | Client bundle data and asset discovery | `old-architecture/source/core/asset_paths.*` and app startup reads | `octaryn-client/Source/Native/AssetPaths/`, client launch probes | Asset-root reads are active for atlas metadata; bundle-root reads still need to cover module `Data/` payloads without workspace fallbacks. |
| 2 | Native atlas upload and material rendering | `old-architecture/source/render/atlas/`, `old-architecture/source/render/world/`, atlas shaders | client rendering, shaders, asset tooling | Basegame atlas PNGs and metadata are bundled and validated; the graphical client probe still validates presentation pixels without the real atlas upload path. |
| 3 | Player movement, collision, spawn, camera state | `old-architecture/source/app/player/`, `old-architecture/source/core/camera/camera.*`, `old-architecture/source/physics/` | basegame movement rules, server physics, client prediction/presentation | Block selection rules are probed; authoritative movement/collision and spawn alignment remain unported. |
| 4 | Block raycast and interaction UX | `old-architecture/source/app/overlay/interaction.*`, `old-architecture/source/world/edit/` | shared commands/queries, basegame rules, server edits, client input | Server block edits and basegame support/replacement rules are probed; client raycast target selection and UX flow remain incomplete. |
| 5 | World save metadata, player save, and chunk override format | `old-architecture/source/core/persistence/` | server persistence with shared compatibility contracts | World block overrides and time are present; player state, world metadata, chunk file/cache semantics, and migration replay remain open. |
| 6 | Dynamic fluids and gases | `old-architecture/source/world/edit/water.cpp`, old block/material behavior, and future fluid declarations | basegame declarations, server active-region simulation, client snapshots | Static water/lava catalog data exists; dynamic fluid/gas simulation and budget validation remain open. |
| 7 | Items, inventories, recipes, tags, loot, and product UI | old gameplay/content/UI source | basegame content/gameplay/UI, client retained UI execution | Hand item and basic interaction data exist; full inventory, recipes, tags, loot, HUD/menu/UI contribution flow remain open. |

## Client Ownership

Client owns the playable local application and every presentation concern.

```text
octaryn-client/
  CMakeLists.txt
  Octaryn.Client.csproj
  Source/
    Native/
    Managed/
    Libraries/
    App/
    Audio/
    Display/
    Input/
    Overlay/
    Player/
    ClientHost/
    FrameLoop/
    Window/
    Rendering/
      Atlas/
      Buffers/
      Pipelines/
      Postprocess/
      Resources/
      Scene/
      Ui/
      Visibility/
      World/
    WorldPresentation/
  Assets/
    Icons/
    Textures/
    Ui/
  Shaders/
  Tools/
  Data/
```

Port source candidates:

- `old-architecture/source/app/`
- `old-architecture/source/render/`
- `old-architecture/source/shaders/`
- client-side pieces of `old-architecture/source/world/chunks/build_mesh.*`
- client-side pieces of `old-architecture/source/world/chunks/upload*.cpp`
- client-side pieces of `old-architecture/source/world/runtime/render_descriptors.cpp`
- Exclude DDGI, skylight propagation, lighting architecture, and old CPU skylight behavior until the dedicated lighting plan is approved.

## Server Ownership

Server owns authority, persistence, validation, simulation, and transport hosting.

```text
octaryn-server/
  Octaryn.Server.csproj
  Source/
    Native/
    Libraries/
    Managed/
      Program.cs
      ServerHost.cs
    Tick/
    Simulation/
    World/
      Blocks/
      Chunks/
      Generation/
      Queries/
    Persistence/
    Networking/
    Physics/
    Validation/
  Assets/
  Data/
  Shaders/  # stays empty unless a real server-owned compute/offline shader need appears.
  Tools/
```

Port source candidates:

- `old-architecture/source/world/edit/`
- server-side pieces of `old-architecture/source/world/runtime/`
- server-side pieces of `old-architecture/source/world/chunks/`
- `old-architecture/source/world/jobs/`
- `old-architecture/source/world/generation/`
- `old-architecture/source/physics/`
- server-owned pieces of `old-architecture/source/core/persistence/`
- Exclude DDGI, skylight propagation, lighting architecture, and old CPU skylight behavior until the dedicated lighting plan is approved.

## Basegame Ownership

Basegame is the default bundled game module. It owns high-level game features, game mechanics, content, assets, and default-game behavior. It must interact with the host through shared API contracts only. It should define what the game is, not how the voxel host stores, lights, meshes, saves, streams, replicates, or transports it.

Basegame may define content-facing concepts such as block definitions, item definitions, recipes, tags, loot, features, biome rules, player/game rules, interaction rules, and content data. It must not contain hard-coded core voxel host concepts such as chunk storage, mesh generation, light propagation, persistence implementation, replication internals, transport code, or direct client/server internals.

Basegame must expose a module manifest and registration entry point. The host validates that manifest before activating basegame content, the same way it will validate future game modules or mod-like packages.

```text
octaryn-basegame/
  Octaryn.Basegame.csproj
  Source/
    Native/
    Libraries/
    Managed/
      GameContext.cs
      ManagedGameTag.cs
    Module/
      BasegameModuleRegistration.cs
    Content/
      Biomes/
      Blocks/
      Features/
      Fluids/
      Items/
      LootTables/
      Materials/
      Recipes/
      Tags/
    Gameplay/
      Entities/
      Actions/
      Interaction/
      Inventory/
      MovementRules/
      Player/
      Time/
      Rules/
  Assets/
    Atlases/
    Blockstates/
    Models/
    Shaders/
    Textures/
  Data/
    Blocks/
    Items/
    Materials/
    Recipes/
    Tags/
    Features/
  Shaders/
  Tools/
```

Port source candidates:

- `docs/migration/gameplay-migration-map.md`
- content definitions from `old-architecture/source/world/block/`, after stripping storage, lighting, mesh, and old host-state details.
- high-level game-rule portions of `old-architecture/source/app/player/`.
- high-level interaction-rule portions of `old-architecture/source/world/edit/`; authoritative edit execution stays server-owned.
- Existing `skylightOpacity` values may stay in basegame block content as metadata, but do not expand them into DDGI, skylight propagation, or lighting host contracts until the dedicated lighting plan is approved.
- biome, feature, and terrain rule data from `old-architecture/source/world/generation/`; generation execution stays server-owned and core noise/chunk internals do not move into basegame.
- unmanaged bridge entry points must stay in host-owned code such as `octaryn-client`; basegame exposes a module registration and manifest only.
- `Octaryn.Basegame.csproj` is the active basegame project name. Do not reintroduce `Octaryn.Game` for the bundled module.
- `Gameplay/Actions/` is for game action declarations and bindings after client input is translated through approved APIs; raw device input remains client-owned.
- `Gameplay/MovementRules/` is for high-level game movement/collision rules; physics execution and authoritative simulation remain server-owned, with client prediction copies owned by client.

## Shared Ownership

Shared owns small contracts and value types used across client, server, basegame, and future game modules. It must not own runtime policy, rendering, persistence implementation, or gameplay behavior. It does own the API shapes that allow the host to load and validate game modules without knowing their internals.

```text
octaryn-shared/
  Octaryn.Shared.csproj
  Source/
    Native/
    Managed/
    Libraries/
    ApiExposure/
    FrameworkAllowlist/
    Time/
    World/
    Networking/
    GameModules/
    ModuleSandbox/
    Compatibility/
    Math/
    Diagnostics/
  Assets/
  Data/
  Shaders/
  Tools/
```

Shared `Assets/`, `Data/`, `Shaders/`, `Tools/`, and `Source/Libraries/` are placeholders only until a pure contract/value need is approved. `Source/Native/HostAbi/` may contain pure ABI layout/version contracts for owner bridges; it must not accumulate runtime implementation, scanners, asset processors, shaders, native support libraries, or gameplay policy.

Port source candidates:

- `old-architecture/source/world/direction.h`
- value-type pieces of `old-architecture/source/world/block/block.h`
- snapshot/command shapes from `old-architecture/source/api/`
- time value types from `old-architecture/source/core/world_time/`

## Game Module Loading And Compatibility

Octaryn should behave like a host for game modules. `octaryn-basegame` is just the bundled default module, not a privileged place for core host logic. Future games, mods, or content modules should follow the same contract shape.

Shared contracts define:

- Module identity: stable module ID, display name, version, API version, and dependency ranges.
- Required capabilities: client presentation features, server authority features, content systems, asset kinds, networking needs, and optional tool requirements.
- API exposure: a manifest-declared list of host API IDs the module requests, with each API granted or rejected by compatibility validation.
- Package exposure: manifest-declared lists of runtime packages, build/analyzer packages, and framework API groups the module requests, checked against allowlists before load.
- Registrations: blocks, items, materials, recipes, tags, loot, features, biomes, interactions, commands, snapshots, and game rules through API-owned registries.
- Compatibility declarations: supported host/API versions, required modules, incompatible modules, multiplayer compatibility, save compatibility, and asset compatibility.
- Validation reports: structured errors and warnings that tools, client, and server can show without loading unsafe gameplay code.

Sandbox and API exposure policy:

- Module APIs are least-privilege and capability-scoped. A module only receives APIs accepted from its manifest.
- In-process modules are an API isolation boundary, not a security boundary for hostile code. Truly untrusted code must run out-of-process under a separate module host or be rejected.
- Modules must not access raw client/server internals, native pointers, persistence backends, transport sockets, GPU resources, process state, threads, filesystem paths, reflection, dynamic assembly loading, unmanaged interop, or network endpoints except through explicit host APIs.
- C# ECS and host networking implementations may cross into C/C++ only through explicit owner host bridges. Modules still see capability-scoped shared APIs, not bridge internals, native pointers, transport packages, or interop surfaces.
- Host APIs expose stable IDs, immutable value types, validated commands, queries, snapshots, registries, events, and bounded resource handles instead of mutable implementation objects.
- Host scheduling APIs expose bounded scheduled work scopes instead of direct threads. Modules request work through the host, and the coordinator places eligible logic on the worker pool.
- Server APIs own authoritative simulation, saves, validation, world edits, and multiplayer state. Client APIs own presentation, input, audio, UI, prediction views, and assets.
- Tools may validate and package module assets offline, but tool access does not imply runtime access.
- Any new host capability required by a module must be added as a small shared contract and implemented by the correct owner before modules can request it.

Enforcement points:

- Shared contracts currently define `GameModuleManifest`, host API ID constants, module capability ID constants, runtime/build package allowlists, framework API group allowlists, denied sandbox group IDs, `ModuleValidationReport`, timing/input-only host frame contracts, and narrow host request command contracts. Typed request records can replace raw manifest string lists once external module packaging needs version ranges and richer diagnostics.
- `octaryn-shared/Source/ApiExposure/` owns API exposure contract shapes and capability names. It does not grant permissions by itself.
- `octaryn-shared/Source/FrameworkAllowlist/` owns allowlist contract shapes for package IDs and framework API groups. It does not scan assemblies by itself.
- `octaryn-shared/Source/ModuleSandbox/` owns sandbox policy contracts and validation result types. It does not contain client/server enforcement logic.
- Client/server/tool hosts currently enforce the contracts with MSBuild/package checks, resolved asset graph checks, source scans, post-build binary metadata checks, manifest validation, and pre-load activation checks. Richer artifact identity binding remains Phase 0 enforcement work before external binary-only modules are trusted.
- Runtime access must go through capability handles supplied by the activating host. Modules must not discover services by scanning assemblies, globals, or implementation namespaces.

.NET package allowlist policy:

`octaryn-shared` must stay package-free or BCL-only unless a contract-only dependency is deliberately approved in this plan. Module-facing contracts must use Octaryn-owned value types and interfaces. Do not expose third-party package types in shared public APIs.

| Package or API group | Allowed owners | Purpose | Version policy | Runtime scope | Validation rule | Enforcement location |
| --- | --- | --- | --- | --- | --- | --- |
| `Arch` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Intended managed ECS for gameplay, basegame, modules, mods, and approved owner-local managed worlds. | Exact central pin in `Directory.Packages.props`. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types; host/native backends meet Arch-managed worlds through Octaryn descriptors and validators. | Module/host package validation and project restore checks. |
| `Arch.LowLevel` | Transitive package for approved Arch runtime packages. | Low-level Arch runtime support. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `Arch.System` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Intended managed ECS system authoring/execution support, driven by Octaryn host scheduling declarations. | Exact central pin. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types; systems must declare Octaryn phases and read/write sets. | Module/host package validation and project restore checks. |
| `Arch.System.SourceGenerator` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Compile-time ECS system generation. | Exact central pin. | Build/analyzer only. | Must be listed as a requested build package and use `PrivateAssets=\"all\"`, `OutputItemType=\"Analyzer\"`, and `IncludeAssets=\"analyzers;build;buildTransitive\"`. | MSBuild/package validation and resolved asset validation. |
| `Arch.EventBus` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Gameplay and host integration events. | Exact central pin. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types. | Module/host package validation and project restore checks. |
| `Arch.Relationships` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Gameplay and host entity relationships. | Exact central pin. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types. | Module/host package validation and project restore checks. |
| `Collections.Pooled` | Transitive package for approved Arch runtime packages. | Pooled collection implementation used by Arch. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `CommunityToolkit.HighPerformance` | Transitive package for approved Arch runtime packages. | High-performance memory/collection primitives used by Arch. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `Microsoft.Extensions.ObjectPool` | Transitive package for approved Arch runtime packages. | Object pooling used by Arch package graph. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `ZeroAllocJobScheduler` | Transitive package for approved Arch runtime packages. | Scheduler support used by Arch.System. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation plus source and binary API denial for `Schedulers.*`. |
| `Humanizer.Core` | Transitive build package for approved source generators. | Source-generator support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.Bcl.AsyncInterfaces` | Transitive build package for approved source generators. | Source-generator support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.Analyzers` | Transitive build package for approved source generators. | Analyzer support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.Common` | Transitive build package for approved source generators. | Compiler API support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.CSharp` | Transitive build package for approved source generators. | C# compiler API support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.CSharp.Workspaces` | Transitive build package for approved source generators. | Workspace support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.Workspaces.Common` | Transitive build package for approved source generators. | Workspace support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.NETCore.Platforms` | Transitive build package for approved source generators. | Platform metadata support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `NETStandard.Library` | Transitive build package for approved source generators. | Reference assembly support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.AttributedModel` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.Convention` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.Hosting` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.Runtime` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.TypedParts` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| Safe BCL value APIs | `octaryn-shared`, `octaryn-basegame`, approved full games/game modules/mods | Primitives, collections, spans/memory, math/numerics, dates/times, text, and diagnostics abstractions. | Runtime-provided .NET 10 APIs only. | Shared contracts and module implementation. | Allowed namespace/API group only; no filesystem, network, reflection, process, environment, or threading APIs. | Analyzer/source scan and pre-load validation. |
| Host-routed JSON/data parsing | `octaryn-basegame`, approved full games/game modules/mods, tools | Content data parsing through approved host APIs. | Runtime-provided .NET 10 APIs unless a package is explicitly approved. | Offline tools or bounded host API runtime path. | Module cannot open arbitrary paths; host supplies bounded data streams/handles. | Tool validation and host capability checks. |
| `LiteNetLib` | `octaryn-client`, `octaryn-server` | Reliable UDP transport. | Exact central pin. | Host transport only. | Rejected in `octaryn-shared`, `octaryn-basegame`, game modules, and mods. | Package validation and owner project checks. |
| `LiteEntitySystem` | `octaryn-client`, `octaryn-server` | Host-side entity replication/synchronization backend behind Octaryn networking contracts. | Exact central pin. | Host implementation only. | Rejected in `octaryn-shared`, `octaryn-basegame`, game modules, and mods; public entities, RPCs, SyncVars, and replication declarations must be Octaryn API shapes. | Package validation and owner project checks. |

Denied to modules by default: `System.IO`, raw filesystem paths, `System.Net`, sockets, HTTP clients, `System.Diagnostics.Process`, unmanaged interop, unsafe native bridges, reflection/dynamic loading, runtime code generation, arbitrary threading/task scheduling, timers, custom worker pools, environment variables, direct host service discovery, direct console/stdout/stderr writes, and unlisted NuGet packages. If a game or mod needs a new package or framework API group, add it here with owner, purpose, version policy, allowed runtime scope, validation rule, and enforcement location before using it.

Package validation currently has two layers:

- MSBuild item validation catches direct package misuse, host-only package misuse, and analyzer metadata mistakes.
- `tools/validation/validate_module_manifest_packages.py` checks that module manifest requested package lists match the module project’s direct runtime/build `PackageReference`s.
- `tools/validation/validate_module_manifest_files.py` checks that module manifest content/assets point at real module files and that non-placeholder `Data/`, `Assets/`, and `Shaders/` files are declared.
- `octaryn-basegame/Data/Module/octaryn.basegame.module.json` is the checked-in bundled module package descriptor. It mirrors `BasegameModuleRegistration.Manifest`, is copied to client/server bundles, and is compared by `tools/validation/Octaryn.ModuleManifestProbe/` so package metadata can become the future discovery source without replacing the current in-process registration path yet.
- `tools/validation/Octaryn.ModuleManifestProbe/` writes the generated validation manifest under `build/<preset>/basegame/generated/octaryn.basegame.manifest.json` for CMake/manual validation and compares the checked-in package descriptor with the code manifest.
- `tools/validation/validate_all_project_reference_boundaries.py` discovers every active `.csproj` under shared, client, server, basegame, tools, games, modules, and mods, then applies `validate_project_reference_boundaries.py` so new projects cannot bypass owner-reference rules. Modules may reference shared contracts only.
- `tools/validation/validate_dotnet_package_assets.py` is owner-aware. It parses module and host `project.assets.json` target graphs, rejects unclassified direct, runtime-transitive, build-direct, and build-transitive packages for modules, and rejects unclassified or unapproved direct packages in client/server host graphs. Shared and old-architecture projects stay outside resolved-package validation unless a future policy explicitly opts them in.
- `tools/validation/Octaryn.ModuleApiProbe/` scans module C# source with Roslyn before compile and rejects denied filesystem, networking, process, reflection, native interop, console, environment, dynamic-loading, raw-threading APIs, transitive scheduler APIs, and unapproved shared networking contracts. Its denied groups must stay aligned with `DeniedFrameworkApiGroups`.
- `tools/validation/validate_module_layout.py` checks module `Data/`, `Assets/`, and `Shaders/` layout, rejects unsupported file suffixes, duplicate normalized file IDs, and empty content/asset/shader files.
- `tools/validation/validate_package_policy_sync.py` keeps `tools/package-policy/module-packages.json`, `ModulePackagePolicy.props`, `Directory.Packages.props`, and shared package allowlist constants synchronized.
- `tools/package-policy/module-packages.json` is the machine-readable package policy source used by the asset graph validator. MSBuild and shared constants must stay aligned with it until those surfaces are generated from the policy file.

External full game projects should live under `octaryn-games/`, external module projects under `octaryn-modules/`, and external mod projects under `octaryn-mods/`, or set an owner through an early-imported props file before the root `Directory.Build.props` is evaluated. Setting `OctarynModuleOwner` only in the `.csproj` body is too late for output routing.

Client/server hosts own activation:

- The server validates authoritative content, gameplay rules, save compatibility, command shapes, snapshot shapes, and multiplayer compatibility before simulation starts.
- The current server project validates the bundled basegame manifest and server compatibility before returning from `ServerHost.Run`; deeper authoritative content/save/multiplayer validation is still a porting task under `octaryn-server/Source/Validation/`.
- The client validates assets, presentation capabilities, UI/overlay declarations, local prediction hooks, and client-compatible snapshot views before presenting a module.
- Tools validate content and assets offline before packaging a game module.
- Basegame and future modules register high-level content and mechanics only after the host accepts their manifest.

No module may bypass shared registries or write directly into client/server/core voxel internals. If a gameplay feature needs a new host capability, add a small shared API contract and implement it in the correct client or server owner.

## Support Libraries

Do not create a generic runtime root. The old native tree already has focused support libs for logging, diagnostics, memory, jobs, dependency wrappers, profiling, and shader tooling. During the port, keep those as small build targets owned by the layer that needs them.

Agent workers must check this library catalog before implementing any feature. If a listed library matches the job, use it through the owner that is allowed to depend on it. Do not hand-roll replacement systems, add parallel dependencies, or move a library across ownership boundaries without updating this plan and the migration map.

Native support landing zones:

- `octaryn-client/Source/ClientHost/`
- `octaryn-client/Source/FrameLoop/`
- `octaryn-client/Source/Managed/`
- `octaryn-client/Source/Libraries/<ExactLibraryName>/` for client-local native support.
- `octaryn-server/Source/Tick/`
- `octaryn-server/Source/Simulation/`
- `octaryn-server/Source/Libraries/<ExactLibraryName>/` for server-local native support.
- `octaryn-shared/Source/Diagnostics/`
- `octaryn-basegame/Source/Libraries/<ExactLibraryName>/` only for basegame-local native support that does not expose host internals.
- `tools/<ExactToolName>/` for repo-wide developer tools that are not owned by a game/content package.
- focused native support targets such as `octaryn_native_logging`, `octaryn_native_diagnostics`, `octaryn_native_memory`, `octaryn_native_profiling`, and `octaryn_native_jobs`.

Port source candidates:

- `old-architecture/source/core/log.*` -> focused diagnostics/logging lib used by client/server.
- `old-architecture/source/core/memory_mimalloc.*` -> native build support linked where needed.
- `old-architecture/source/core/crash_diagnostics.*` -> diagnostics support, not a product root.
- `old-architecture/source/runtime/jobs/` -> client/server job support or server simulation scheduler depending usage.
- native managed-host bridge pieces -> client host or shared contracts after renaming away from engine API names.

## CMake And Platform Build Architecture

Root `cmake/` owns only new-architecture build policy. Old CMake files stay under `old-architecture/cmake/` until intentionally ported into the structure below. Do not move old CMake modules wholesale; split them by responsibility first.

The tree below is the required target structure. In the current workspace these paths are placeholders unless the named `.cmake` file exists. Do not describe Windows, Linux, owner target, dependency, or root preset support as implemented until the concrete module and a targeted configure check exist.

```text
cmake/
  Shared/
    ProjectDefaults.cmake
    CompilerWarnings.cmake
    BuildOutputs.cmake
    OwnerBuildLayout.cmake
  Owners/
    DotNetOwner.cmake
    NativeOwner.cmake
    ClientTargets.cmake
    ServerTargets.cmake
    SharedTargets.cmake
    BasegameTargets.cmake
    ToolTargets.cmake
  Dependencies/
    DependencyPolicy.cmake
    DotNetHosting.cmake
    SourceDependencyCache.cmake
    NativeDependencyAliases.cmake
    ClientDependencies.cmake
    ToolDependencies.cmake
  Platforms/
    PlatformDispatch.cmake
    Windows/
      WindowsPlatform.cmake
    Linux/
      LinuxPlatform.cmake
      ArchFamily.cmake
      DebianFamily.cmake
      FedoraFamily.cmake
      SuseFamily.cmake
  Toolchains/
    Windows/
      clang.cmake
    Linux/
      clang.cmake
```

Layer responsibilities:

- `cmake/Shared/` owns repo-wide CMake defaults: C/C++ standards, warning policy, output layout, build/log owner paths, shared helper functions, and naming rules.
- `cmake/Owners/` owns target construction for client, server, shared contracts, basegame assets/modules, and tools. Owner modules may call shared helpers and dependency aliases, but must not contain platform detection.
- `cmake/Dependencies/` owns dependency wrappers and allowed dependency aliases. Dependencies must be grouped by real owner need; do not recreate one old global dependency bag.
- `cmake/Platforms/` owns host platform facts and distro-family policy: Windows, Linux family package hints, and platform capability checks.
- `cmake/Toolchains/` owns cross/native compiler toolchain files only. Toolchain files set compilers, sysroots, target triples, find-root behavior, and platform knobs; they must not create Octaryn targets or fetch dependencies.
- `tools/build/` owns new developer-facing build commands that select presets/toolchains and write to `build/<preset>/<owner>/` and `logs/<owner>/`.

Platform rules:

- Windows cross-builds from Linux use the explicit Windows Clang toolchain file under `cmake/Toolchains/Windows/clang.cmake`. LLVM MinGW is the implementation behind that toolchain, not a public platform folder or preset name.
- Linux-hosted builds are Clang-only. Public presets are exactly `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`.
- Cross-platform builds are designed to run from Linux/Arch first. Active Podman build wrappers use the Linux-hosted toolchain environment for Linux and Windows targets instead of introducing separate host-specific build layouts; expand those wrappers in place when platform coverage grows.
- Linux policy is split by distro family only when real package/tool behavior differs. Start with Arch, Debian, Fedora, and Suse/openSUSE because the old dependency installer already has distinct package-manager logic for those families.
- Platform modules report capabilities; owner targets decide whether to use those capabilities. Platform modules must not own gameplay, rendering, server, basegame, or module-sandbox behavior.

Port map for old CMake:

- `old-architecture/cmake/BuildLayout.cmake` -> `cmake/Shared/OwnerBuildLayout.cmake`, after renaming away from engine product names and enforcing `build/<preset>/<owner>/` and `logs/<owner>/`.
- Old dependency cache paths such as `build/shared/deps/<bucket>` and `logs/deps/<bucket>` -> shared dependency sources/downloads under `build/dependencies/`, with preset-specific dependency build trees and population stamps under `build/<preset>/deps/`.
- `old-architecture/cmake/ProjectOptions.cmake` -> `cmake/Shared/ProjectDefaults.cmake` plus owner-specific options in `cmake/Owners/`.
- `old-architecture/cmake/Dependencies.cmake` -> `cmake/Dependencies/`, split by dependency policy, alias creation, and owner-specific dependency groups.
- `old-architecture/cmake/CPM.cmake` -> `cmake/Dependencies/` only if CPM remains the selected dependency mechanism.
- `old-architecture/cmake/toolchains/windows-x64.cmake` -> `cmake/Toolchains/Windows/clang.cmake` only as a Windows Clang cross toolchain; GCC-based MinGW is not an active lane.
- `old-architecture/CMakePresets.json` -> new root presets only after the owner/platform/toolchain split exists.
- `old-architecture/tools/build/configure.sh`, `cmake_build.sh`, `build_all.sh`, and repair/install helpers -> `tools/build/` only after they select new owner presets, use the new platform/toolchain modules, and write outputs to `build/<preset>/<owner>/`, `build/<preset>/deps/`, `build/dependencies/`, `logs/<owner>/`, or `logs/build/`.
- `old-architecture/tools/build/tracy_capture.sh` -> focused profiling wrappers under root `tools/` only after logs are moved away from old `logs/engine_control`, `logs/tracy`, and `logs/octaryn-engine` paths. Old RenderDoc helpers stay in `old-architecture/`; RenderDoc is handled by external developer installs.
- Old architecture scripts that remain under `old-architecture/` are source material only and are not accepted validation paths until intentionally ported to the active preset layout.

Validation for CMake changes:

- For structure-only CMake work, run `validate_cmake_target_inventory.py`; it verifies active target names, required owner/platform/dependency files, and absence of old generic product CMake paths.
- For build policy changes, configure the smallest owner target that uses the changed policy.
- For platform/toolchain changes, run targeted configure checks for the affected platform or toolchain when the host has that compiler/sysroot installed.
- Active configure presets are exactly `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`. Linux native targeting is Clang-only in active lanes; GCC is not an active preset lane. Windows targets are cross-built inside the Linux/Arch builder with the Windows Clang toolchain; native Windows developer builds are not an active lane. Windows Clang configure may disable hostfxr bridge/probe targets when target-compatible .NET native hosting assets are unavailable, but Linux host validation must still build and run those bridge/probe targets.
- Do not validate CMake work with smoke tests or `ctest` unless explicitly requested.

## Library Catalog

Use these libraries for the cases they are intended for. Keep wrappers focused and owner-scoped; old `octaryn_engine_*` targets are source-material names only and must not be recreated in the active graph.

### Native Support Targets

| Old target | Purpose | Destination |
| --- | --- | --- |
| `octaryn_engine_log` | Native logging through `spdlog`. | `octaryn_native_logging`, used by client, server, tools, and support libraries that need logs. |
| `octaryn_engine_diagnostics` | Crash diagnostics and stack traces. | `octaryn_native_diagnostics`, used by executables and tools that need crash reports. |
| `octaryn_engine_memory` | Process allocator setup through `mimalloc`. | `octaryn_native_memory`; SDL coupling removed while porting. |
| `octaryn_engine_imgui_backend` | Dear ImGui backend glue for SDL3 and SDL GPU. | Client UI/debug UI only. |
| `octaryn_engine_shader_tool` | GLSL compilation, reflection, validation, and SPIR-V/MSL asset generation. | Root `tools/` shader compiler; generated shaders are client-owned assets. |
| `octaryn_engine_texture_atlas` | Builds block/material texture atlases from basegame content and packs. | `octaryn-basegame/Tools/`; generated atlas assets are consumed by the client. |
| `octaryn_managed_game` | Publishes the C# basegame assembly. | `octaryn-basegame`. |
| `octaryn_engine_shader_assets` | Generated shader asset target. | `octaryn-client` asset build. |
| `octaryn_engine_runtime_assets` | Copies generated assets into a runnable client bundle. | `octaryn-client` packaging. |
| `octaryn_engine_runtime_bundle` | Old monolithic runtime bundle. | Replaced by the active `octaryn_client_bundle`; keep the old name only in migration notes. |
| `octaryn_engine_runtime` | Old monolithic native executable. | Split across client, server, shared, high-level basegame mechanics/content, tools, and support targets. Do not recreate it under a new name. |

### Native Dependency Wrappers

| Wrapper | Purpose | Allowed owners |
| --- | --- | --- |
| `octaryn::deps::spdlog` | Fast structured/native logging. | Native logging support, client, server, tools. |
| `octaryn::deps::cpptrace` | Stack traces for crash diagnostics. | Native diagnostics support and executable crash paths. |
| `octaryn::deps::tracy` | Profiling instrumentation. | Client, server, tools, and native support when profiling is enabled. |
| `octaryn::deps::mimalloc` | Allocator backend. | Native memory support only; consumers use the support target. |
| `octaryn::deps::taskflow` | Job execution substrate. | Native jobs support below Octaryn-owned scheduling policy; modules never see Taskflow types or task graphs. |
| `octaryn::deps::unordered_dense` | High-performance hash maps and sets. | Owner-local implementation code that needs dense hash containers. |
| `octaryn::deps::eigen` | Math and linear algebra. | Shared pure math/value code or owner-local math; rendering-only math stays client-owned. |
| `octaryn::deps::glaze` | JSON and metadata serialization. | Shared contracts when pure, client settings persistence, server persistence, basegame content tools, root tools. |
| `octaryn::deps::sdl3` | Windowing, input, SDL GPU, platform services, timers. | Client only, except isolated tool use when a tool truly needs SDL. |
| `octaryn::deps::sdl3_image` | Image loading. | Client UI/assets and asset import tools. |
| `octaryn::deps::sdl3_ttf` | Text rendering and shaping/fallback support. | Client UI and overlays only; not a product UI framework by itself. |
| `octaryn::deps::imgui` | Immediate-mode runtime/debug UI. | Client UI/debug UI and tools. |
| `octaryn::deps::implot` | 2D plots and telemetry panels. | Client debug UI and tools. |
| `octaryn::deps::implot3d` | 3D plots and debug visualization. | Client debug UI and tools. |
| `octaryn::deps::imgui_node_editor` | Node graph editor UI. | Tools first; client only for explicit debug/editor UI. |
| `octaryn::deps::imguizmo` | Transform gizmos. | Tools first; client only for explicit debug/editor UI. |
| `octaryn::deps::imanim` | ImGui animation/editor widgets. | Tools first; do not ship in core client unless a real client feature uses it. |
| `octaryn::deps::imfiledialog` | ImGui file dialogs. | Tools or explicit client debug/editor UI. |
| `octaryn::deps::shaderc` | GLSL to SPIR-V compilation. | Shader tooling only. |
| `octaryn::deps::shadercross` | SDL shader cross-compilation. | Shader tooling only. |
| `octaryn::deps::spirv_tools` | SPIR-V validation and optimization. | Shader tooling only. |
| `octaryn::deps::spirv_cross` | Shader reflection and cross-compilation. | Shader tooling only. |
| `octaryn::deps::fastgltf` | glTF import/loading. | Asset import tools; client only if runtime glTF loading is intentionally added. |
| `octaryn::deps::ktx` | KTX texture containers and GPU texture pipeline. | Asset tools and intentional client texture loading. |
| `octaryn::deps::meshoptimizer` | Mesh optimization and import processing. | Asset tools; client only for intentional runtime optimization. |
| `octaryn::deps::ozz_animation` | Skeletal animation runtime. | Client animation runtime; basegame may own animation content data. |
| `octaryn::deps::openal` | Planned hidden spatial runtime audio backend. | Client audio; do not expose OpenAL types to modules. |
| `octaryn::deps::miniaudio` | Audio decode, streaming, helper, or tool roles unless benchmarks justify runtime consolidation the other way. | Client audio helpers and tools; do not keep as an equal first-class runtime backend indefinitely. |
| `octaryn::deps::zlib` | Compression. | Server persistence, asset tools, and support wrappers. |
| `octaryn::deps::lz4` | Fast save/cache compression. | Server persistence and tooling caches. |
| `octaryn::deps::zstd` | Strong save/cache compression. | Server persistence and tooling caches. |

Planned dependency decisions not yet implemented as active CMake coverage:

| Candidate | Decision | Owner boundary |
| --- | --- | --- |
| Jolt | First physics backend candidate. | Hidden under `octaryn-server` authority and client prediction/presentation wrappers; modules see only Octaryn physics APIs. |
| Yoga | First retained-UI layout solver candidate. | Hidden under `octaryn-client` UI execution; modules see only Octaryn UI declarations. |
| RmlUi | Deferred candidate only if a user-approved UI authoring plan chooses markup-style product UI. | No active dependency or API surface; must stay behind Octaryn UI contracts if adopted later. |
| FlatBuffers | Optional control-plane schema candidate. | Not the primary voxel/chunk/entity save format. |
| Recast/Detour | Later navigation candidate. | Defer until world, physics, persistence, and tool spines are stable. |

### Managed Packages

| Package | Purpose | Allowed owners |
| --- | --- | --- |
| `Arch` | Intended managed ECS for gameplay, basegame, modules, mods, and approved owner-local managed worlds. | `octaryn-basegame`; client/server only for private owner-local state, never `octaryn-shared`. |
| `Arch.System` | Intended managed ECS system authoring/execution support, driven by Octaryn host scheduling declarations. | Same as `Arch`; do not expose scheduler internals or raw threading to modules. |
| `Arch.System.SourceGenerator` | Compile-time support for `Arch.System`. | Only projects that directly define Arch systems; keep analyzer/private. |
| `Arch.EventBus` | Gameplay event bus for Arch-style systems. | `octaryn-basegame` or private owner-local systems, never shared contracts. |
| `Arch.Relationships` | ECS entity relationship helpers. | `octaryn-basegame` or private owner-local systems, never shared contracts. |
| `LiteNetLib` | Reliable UDP transport. | `octaryn-client` and `octaryn-server` transport implementation only. |
| `LiteEntitySystem` | Host-side entity replication/synchronization backend behind Octaryn networking contracts. | `octaryn-client` and `octaryn-server` only; never shared/basegame/module public APIs. |

`LiteNetLib` and `LiteEntitySystem` stay centrally pinned for host networking work, but they are not module permissions. Basegame must talk through shared command, snapshot, registry, interaction, feature, query, and scheduling contracts rather than transport packages, raw threads, or client/server internals.

Host-owned C# ECS or networking packages may be driven from C/C++ through narrow client/server bridge contracts when that is the best owner route. That does not make those packages module-facing or weaken the module sandbox.

Sandboxed game modules and mods must not reference host-only packages or unlisted NuGet packages. `Directory.Packages.props` is only the central version pin file; it is not permission to use a package from module code.

## Old Architecture Tooling

Old build helpers, old CMake modules, old desktop helper tools, and old profiling wrappers stay under `old-architecture/` until they are intentionally ported.

The old atlas builder is basegame-specific content tooling and belongs under `octaryn-basegame/Tools/AtlasBuilder/` or another focused basegame tool folder. Keep a one-file script only if it stays small and deliberately scoped.

Active root `cmake/` and `tools/` are reserved for new architecture support only. Generated outputs should be owner-partitioned under `build/<preset>/<owner>/` and `logs/<owner>/`.

Bundle composition rules:

- `octaryn_client_bundle` is the graphical client package. It must include validated client assets, shared contracts, basegame/module payloads, and the version-matched `server/` payload required for local singleplayer worlds.
- `octaryn_server_bundle` is the dedicated headless server package. It must include server authority, shared contracts, basegame/module payloads, and server-side validation without any client presentation payload.
- Server files copied into the client bundle must originate from server-owned build outputs. Do not compile those server files as client-owned implementation.
- Future bundle validators must reject missing `server/` payloads in the client bundle and reject client rendering/window/audio/UI payloads in the dedicated server bundle.

## Build Target Names

These are CMake/build target names, not necessarily shipped executable or folder names. Shipped local singleplayer naming stays `client_server_app` with a bundled `server/` payload.

Active root targets:

```text
octaryn_shared
octaryn_shared_native
octaryn_shared_host_abi
octaryn_native_logging
octaryn_native_diagnostics
octaryn_native_memory
octaryn_native_profiling
octaryn_native_jobs
octaryn_basegame
octaryn_basegame_native
octaryn_basegame_bundle
octaryn_server
octaryn_server_bundle
octaryn_server_native
octaryn_server_managed_bridge
octaryn_server_launch_probe
octaryn_client_managed
octaryn_client_native
octaryn_client_asset_paths
octaryn_client_app_settings
octaryn_client_camera
octaryn_client_camera_matrix
octaryn_client_display_catalog
octaryn_client_display_menu
octaryn_client_display_settings
octaryn_client_frame_pacing
octaryn_client_fullscreen_display_mode
octaryn_client_frame_metrics
octaryn_client_hidden_block_uniforms
octaryn_client_host_environment
octaryn_client_lighting_settings
octaryn_client_render_distance
octaryn_client_shader_creation
octaryn_client_shader_metadata_contract
octaryn_client_shaders
octaryn_client_swapchain
octaryn_client_visibility_flags
octaryn_client_window_frame_statistics
octaryn_client_window_lifecycle
octaryn_client_managed_bridge
octaryn_client_launch_probe
octaryn_client_server_app
octaryn_client_bundle
octaryn_tools
octaryn_shader_compiler
octaryn_debug_tools
octaryn_all
octaryn_validate_all
octaryn_validate_cmake_targets
octaryn_validate_cmake_policy_separation
octaryn_validate_cmake_dependency_aliases
octaryn_validate_package_policy_sync
octaryn_validate_project_references
octaryn_validate_module_manifest_packages
octaryn_validate_module_manifest_files
octaryn_validate_module_manifest_probe
octaryn_validate_bundle_module_payload
octaryn_validate_client_server_app
octaryn_client_server_app_launch_probe
octaryn_validate_client_shader_bundle
octaryn_validate_module_source_api
octaryn_validate_module_binary_sandbox
octaryn_validate_module_layout
octaryn_validate_basegame_block_catalog
octaryn_validate_basegame_worldgen_content
octaryn_validate_dotnet_package_assets
octaryn_validate_native_abi_contracts
octaryn_validate_native_owner_boundaries
octaryn_validate_native_archive_format
octaryn_validate_dotnet_owners
octaryn_validate_scheduler_contract
octaryn_validate_scheduler_probe
octaryn_validate_world_time_probe
octaryn_validate_owner_module_validation_probe
octaryn_validate_server_world_blocks_probe
octaryn_validate_server_world_generation_probe
octaryn_validate_basegame_player_probe
octaryn_validate_basegame_interaction_probe
octaryn_validate_client_world_presentation_probe
octaryn_validate_hostfxr_bridge_exports
octaryn_validate_owner_launch_probes
octaryn_run_client_launch_probe
octaryn_run_server_launch_probe
```

Internal dependency targets such as `octaryn_dotnet_hosting` and `octaryn_native_threads` are implementation details for owner CMake modules. They are not public build targets, but they must stay under `cmake/Dependencies/` and remain out of old-architecture target wiring.

Planned focused targets, added only when their implementation exists:

```text
octaryn_client_assets
octaryn_basegame_assets
octaryn_module_validator
octaryn_module_sandbox_contracts
```

## Validation

- Do not use smoke tests unless explicitly requested.
- Do not run `ctest` unless explicitly requested.
- Use direct runtime launches, targeted benchmarks, Tracy captures, focused logs, and external GPU captures when a developer has a local capture tool installed.
- For reference-parity work, inspect Minecraft, Iris, and Complementary Reimagined before implementing behavior.
- For architecture-only structure work, verify file tree shape, empty-file/placeholders, owner partitioning, and absence of stale old product names in active roots.
- For every touched old-architecture file, verify the source-to-destination map or a removal reason is recorded.
- For module/API/package work, verify no unapproved package, transitive package, framework API group, unsafe/native bridge, direct console write, reflection, filesystem, network, process, threading, or dynamic loading access was introduced.
- For CMake work, verify old dependency/log paths are not recreated in active roots and targeted configure checks cover the changed owner/platform/toolchain when practical.

## Phase Order

1. Create blank owner structure and migration maps.
2. Rename managed API away from `Octaryn.Engine.Api` into shared contracts and client/server host contracts.
3. Define shared game-module manifest, API exposure, package allowlist, compatibility, validation, registry, command, snapshot, and query contracts.
4. Define the host scheduling contract: main thread, coordinator thread, scalable worker pool with at least two workers, scheduled system declarations, and thread-safety rules for all computation and gameplay logic.
5. Create the new CMake split: shared build policy, owner targets, dependency aliases, platform modules, and toolchains.
6. Split build target names away from `octaryn_engine_*`.
7. Port shared value contracts.
8. Port server-authoritative world and persistence behind shared APIs and coordinator-scheduled worker jobs.
9. Port client presentation, windowing, rendering, shaders, and uploads behind shared APIs, keeping computation on worker jobs and presentation handoff on client-owned main-thread paths.
10. Port basegame as the first validated game module with high-level content and gameplay rules only, scheduled through host APIs.
11. Wire client-server transport through shared message contracts.
12. Validate module loading, package allowlist checks, API exposure checks, scheduler/thread-safety checks, CMake platform/toolchain checks, compatibility checks, and runtime/profiling paths.

For the current non-lighting old-architecture port loop, use the queue near the top of this appendix to choose phase 8-10 slices. Start with bundle-root data/asset discovery, then real atlas upload, then player movement/raycast/persistence before dynamic fluids and larger inventory/UI work.

Lighting hold:

- DDGI is the intended future lighting direction, but it is not active work until the user provides a dedicated plan.
- Do not port old CPU skylight propagation as the lighting implementation path.
- Do not add interim server lighting contracts, DDGI probes, or client lighting rewrites while this hold is active.
- Treat old skylight files as reference/source material only for future planning, not as the next port target.
