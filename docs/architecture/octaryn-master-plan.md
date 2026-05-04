# Octaryn Master Plan

This is the canonical master plan for Octaryn's architecture, ECS substrate, game-module API, basegame/mod separation, owner layout, build layout, validation policy, and current library direction.

The target is simple for creators: define the thing, attach components, write allowed logic, and declare replication/persistence intent. The host owns the backend ECS, scheduling, networking, persistence, native execution, validation, and presentation handoff.

If this file conflicts with `docs/architecture/octaryn-appendix.md`, this file wins. `octaryn-appendix.md` remains an appendix and migration checklist, not a competing source of architecture truth. `AGENTS.md` remains the execution rulebook for agents working in the repository.

## Master Plan Inputs

- `AGENTS.md`: agent execution rules, owner boundaries, finish checks, validation prohibitions, and no-generic-bucket rules.
- `docs/architecture/octaryn-appendix.md`: supplemental overview, current constraints, boundaries, launch rules, and the non-lighting port queue. Its focused sibling appendix files own owner-root maps, module policy, build/library policy, target inventory, validation, and phase order.
- `/home/zacharyr/Downloads/deep-research-report.md`: external research input, not policy by itself. Project corrections override raw report recommendations where they differ: Arch ECS, LiteNetLib, and LiteEntitySystem stay in use; DDGI/skylight waits for a dedicated user plan; names stay `client_server_app` and `server`.
- Current repo state: active owners are `octaryn-client/`, `octaryn-server/`, `octaryn-shared/`, `octaryn-basegame/`, root `tools/`, root `cmake/`, `docs/`, `refrances/`, and `old-architecture/` as source material only.

## Source Priority

1. This master plan owns architecture decisions, API shape, ECS direction, module policy, dependency decisions, validation gates, and phase order.
2. `AGENTS.md` owns how agents execute work: inspect first, plan briefly, preserve owner boundaries, avoid smoke tests and `ctest`, and validate directly.
3. `octaryn-appendix.md` owns supplemental source-to-destination checklists until fully merged here. It must be updated to match this plan, never the other way around.
4. Old architecture and external references are source material only. They do not define destination folders, names, or authority boundaries.

## Current Repo State

- The active repository root is `/home/zacharyr/octaryn-workspace`.
- The old `octaryn-engine/` tree is deleted from the active working structure and preserved as tracked `old-architecture/` source material.
- `octaryn-client/`, `octaryn-server/`, `octaryn-shared/`, and `octaryn-basegame/` have real owner project files and must remain separate.
- `octaryn-client/` owns client host exports, client-owned basegame/module activation, client presentation, and native client bridge edges.
- `octaryn-server/` owns server host exports, server module activation, server validation, authority, persistence, and future transport hosting.
- `octaryn-basegame/` contains the current managed game context, basegame module registration, content, gameplay rules, and basegame-owned tools/data.
- `octaryn-shared/` contains implementation-free contracts: timing/input host-frame contracts, command/snapshot shapes, module manifests, dependency/content/asset declarations, host API IDs, capability IDs, allowlist records, sandbox policy records, and manifest validation types.
- Root MSBuild policy rejects unknown owners, package references in shared, host-only packages outside client/server, unapproved direct module packages, analyzer packages with runtime assets, unapproved resolved runtime/analyzer packages for module owners, and unclassified packages in module assets.
- Active `cmake/` has a new-architecture scaffold: root CMake files, owner modules, dependency policy placeholders, platform modules, toolchain files, and root build wrappers. It must not be described as parity with the old monolith until targeted checks prove each lane.
- Root `tools/` owns repo-wide build, validation, profiling, launch, and developer operations. Basegame-specific content tooling belongs under `octaryn-basegame/Tools/`.
- `docs/` is documentation-only. `refrances/` is reference material, including Minecraft, Iris, and Complementary Reimagined checkouts. `old-architecture/.octaryn-cache/` is ignored generated/reference cache unless explicitly promoted.

## Phase 0 Blockers

These are current migration blockers. Do not add or expand module-facing behavior that depends on them. Work touching these areas must remove the blocker, add enforcement, or keep the affected behavior non-activated.

- Keep `octaryn-basegame` on `octaryn-shared` contracts. Do not reintroduce references to old `Octaryn.Engine.Api` projects or namespaces.
- Keep unmanaged managed-host exports in host-owned client/server code, not basegame.
- Keep `AllowUnsafeBlocks` out of `octaryn-basegame`; module code must not normalize unsafe/native bridge access.
- Keep unsafe native function-pointer bridges out of `octaryn-shared`; shared exposes safe module contracts, not raw host ABI types.
- Keep host-only packages out of `octaryn-basegame`; `LiteNetLib` and `LiteEntitySystem` belong only in client/server when transport is wired.
- Keep LiteEntitySystem hidden behind Octaryn networking contracts. It is kept as a host-side implementation component, not a module-facing API.
- Keep SDK project definitions and owner-routed outputs for `Octaryn.Client.csproj`, `Octaryn.Server.csproj`, `Octaryn.Shared.csproj`, and `Octaryn.Basegame.csproj`.
- Keep pre-load manifest validation file-backed: content declarations must point at existing `Data/`, asset declarations must point at existing `Assets/` or `Shaders/`, and undeclared content/assets must fail validation.
- Replace runtime `legacy*` content schema fields with stable Octaryn IDs or generator-only migration metadata before basegame catalogs are treated as final module data.
- Keep resolved transitive package validation enforced for basegame and extend the same runtime/build-analyzer allowlist model to external modules when those projects are introduced.
- Keep source-level framework API allowlist enforcement and post-build binary metadata inspection active for namespaces, types, and members.
- Keep artifact identity, package/content binding, signature/hash policy, and multiplayer compatibility validation as blockers before external binary-only modules are trusted.
- Keep the owner thread contract enforced before heavy compute systems move: one main thread, one coordinator thread, and a scalable worker pool with at least two workers.
- Expand native owner CMake targets and targeted platform configure checks before claiming native platform/toolchain parity with the old monolith.
- Hostfxr bridge readiness requires exact managed method-name resolution, ABI size/version validation, owner bundle discovery, failure-path validation, and direct runtime launch evidence.

## Core Direction

- The core host baseline is intentionally minimal: a flying camera, no built-in player physics, and flat blank terrain. Physics, terrain features, game movement, interaction rules, entities, items, UI, and progression come from explicit owner systems and game-module declarations.
- Target worlds are 512 blocks tall so the vertical span can be centered around the world origin. Current 256-height and chunk-edge placeholder constants are migration debt; future world constants should separate chunk width/depth from world height instead of deriving height from chunk edge length.
- ECS is the substrate for blocks, items, entities, UI state, global game state, world interactions, fluids, gases, machines, projectiles, abilities, and future content systems.
- Arch ECS owns the managed gameplay/module ECS layer. C++ owns high-throughput host storage, scheduling execution substrates, native simulation kernels, networking packers, persistence packers, and world interaction pipelines where managed ECS should not own the hot path.
- `octaryn-shared` owns only explicit API contracts, IDs, declarations, capability handles, system declarations, and validation-facing shapes.
- `octaryn-basegame` is the first bundled game module. It uses the same public API model as future game modules and mods.
- Game modules and mods never receive raw client, server, renderer, networking, filesystem, scheduler, native pointer, or ECS internals.
- Modules declare what they need. Hosts decide whether the declaration is allowed, how it maps to native ECS, and where it runs.

Design sentence:

> Game modules define content, components, behavior systems, replication, persistence, and capabilities through shared APIs; client/server hosts map those declarations into Arch-managed worlds, native owner storage, scheduling, networking, persistence, prediction, and presentation pipelines.

## Adopted Research Direction

`/home/zacharyr/Downloads/deep-research-report.md` confirms the direction: Octaryn should not become an off-the-shelf runtime with public ECS, physics, networking, UI, or save-framework concepts leaking into modules. The final architecture is an Octaryn-owned platform API with hidden owner backends.

Adopted planning decisions:

- ECS is a hybrid architecture. Arch ECS stays as the intended managed ECS for gameplay, basegame, modules, and approved owner-local managed systems. Native owner ECS/storage exists for high-throughput authority, presentation, replication packing, persistence packing, and world kernels where C++ is the right tool.
- Taskflow remains a hidden execution substrate. Octaryn scheduling policy owns tick phases, read/write sets, barriers, cancellation, profiling zones, and deterministic ordering.
- Physics should plan around Jolt as the first backend candidate, hidden behind Octaryn physics declarations, queries, events, and intents. PhysX is not active work; revisit it only under a user-approved physics plan if Jolt fails a concrete requirement. Modules never see raw backend bodies, worlds, shapes, or query handles.
- Product UI should be a custom retained Octaryn UI model with hidden Yoga layout and SDL3_ttf text rendering. RmlUi is not active work; revisit it only under a user-approved UI authoring plan if retained UI cannot cover the product needs. ImGui remains debug/tool UI, not product UI.
- Networking should use LiteNetLib and LiteEntitySystem as hidden client/server implementation components behind Octaryn-owned command, snapshot, replication, prediction, and compatibility APIs. Octaryn replication descriptors, protocol, authority, and public contracts remain canonical. Modules declare networking intent through Octaryn contracts; they do not reference transport, entity, RPC, SyncVar, or replication backend types directly.
- Persistence should use custom binary sectioned save formats for hot world, chunk, ECS, inventory, journal, and entity data. JSON through Glaze is for manifests and inspectable metadata. LZ4 is for hot chunks/caches; Zstd is for colder saves, backups, and transfer. FlatBuffers may be evaluated for control-plane envelopes only; Protobuf and Cap'n Proto are not primary voxel save formats.
- Runtime audio should converge on one hidden client runtime backend before content scale grows. Current plan favors OpenAL Soft for spatial runtime audio and miniaudio for helper, decode, streaming, or tool roles unless benchmarks justify changing that.
- External native mods are not part of the default mod model. Native systems are owner-owned by default. External native code remains undecided: never, first-party only, or signed trusted extensions only.
- Basegame must become "just another validated module" as early as possible, while still shipping as the bundled default game.

These decisions are planning commitments only. Do not claim Jolt, Yoga, new native ECS/storage backends, retained UI, custom save containers, or networking integration are implemented until concrete owner code, CMake wiring, and targeted validation exist.

## Owner Split

| Owner | Responsibility |
| --- | --- |
| `octaryn-shared` | API contracts, IDs, declarations, capability IDs, system phases, component metadata, replication/persistence policies, command/query shapes. |
| `octaryn-basegame` | Default content and gameplay: blocks, items, entities, rules, recipes, interactions, systems, tags, loot, features, and base game state. |
| Game modules/mods | External content and logic using approved shared APIs and declared capabilities only. |
| `octaryn-server` | Authoritative Arch/native ECS worlds, simulation, validation, persistence, replication, networking, entity/world authority, fluids/gases, block edits, saves. |
| `octaryn-client` | Presentation Arch/native ECS worlds, input mapping, UI rendering, interpolation, prediction, local presentation state, renderer/upload handoff. |
| `tools` | Repo-wide validators, schema generators, module inspection, ABI checks, packaging checks, profiling, and shared asset tooling. Basegame-specific content tools belong under `octaryn-basegame/Tools/`. |
| `cmake` | Owner target construction, dependency aliases, platform/toolchain policy, build layout. |

## Destination Roots And Folder Shape

Only these top-level destination roots are active architecture owners:

```text
octaryn-client/
octaryn-server/
octaryn-shared/
octaryn-basegame/
tools/
cmake/
docs/
refrances/
old-architecture/
build/<preset>/<owner>/
build/<preset>/deps/
build/dependencies/
logs/<owner>/
```

Do not add top-level `engine/`, `octaryn-engine/`, generic `runtime/`, `common`, `helpers`, `misc`, or catch-all owners. `docs/` is documentation only. `refrances/` is reference material. `old-architecture/` is tracked source material only and never an active implementation target.

Each main owner root uses the same vocabulary where it applies:

- `Source/`: owner code.
- `Source/<Domain>/`: focused owner behavior/domain code, regardless of implementation language.
- `Source/Libraries/`: small owner-local native libraries before promotion to clearer domain folders.
- `Assets/`: runtime assets owned by that owner.
- `Shaders/`: shader source owned by that owner.
- `Tools/`: tools specific to that owner.
- `Data/`: structured content/config/data specific to that owner.

Top-level `Source/Native/` and `Source/Managed/` buckets are not active owner landing zones. When a focused domain truly needs both C/C++ and C#, place the language-specific files inside that domain. The shared owner has one narrow exception: `octaryn-shared/Source/Native/HostAbi/` may hold pure ABI layout/version contracts for owner bridges.

Shared `Assets/`, `Shaders/`, `Data/`, `Tools/`, and `Source/Libraries/` stay empty unless a pure implementation-free shared need is approved. Server `Shaders/` stays empty unless a real server-owned compute/offline shader need appears. Basegame may own `Assets/`, `Shaders/`, `Data/`, and `Tools/` because it is the bundled content/game module.

Owner landing zones:

```text
octaryn-client/
  CMakeLists.txt
  Octaryn.Client.csproj
  Source/
    App/
    Host/
    Diagnostics/
    Display/
    HostBridge/
    Input/
    Libraries/
    Audio/
    Ui/
    WorldPresentation/
    Window/
    Rendering/
    Settings/
    Prediction/
    Networking/
    Validation/
  Assets/
  Data/
  Shaders/
  Tools/

octaryn-server/
  CMakeLists.txt
  Octaryn.Server.csproj
  Source/
    Host/
    HostBridge/
    Libraries/
    Modules/
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
  Shaders/
  Tools/

octaryn-basegame/
  CMakeLists.txt
  Octaryn.Basegame.csproj
  Source/
    Libraries/
    Module/
    Content/
      Blocks/
      Items/
      Materials/
      Recipes/
      Tags/
      Loot/
    Gameplay/
      Entities/
      Player/
      Interaction/
      GameState/
    Ui/
    Worldgen/
    Validation/
  Assets/
  Data/
  Shaders/
  Tools/

octaryn-shared/
  CMakeLists.txt
  Octaryn.Shared.csproj
  Source/
    Native/
      HostAbi/
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

Concrete file placement beats generic folders. If a subsystem does not fit one of these owners cleanly, split it by authority/presentation/content/contract/tool responsibility before moving it.

## Build, Bundle, And Log Layout

Build outputs are generated and owner-partitioned:

- `build/<preset>/client/`: client builds, generated client assets, graphical bundle, client native artifacts.
- `build/<preset>/server/`: dedicated server builds, server bundle, server native artifacts.
- `build/<preset>/basegame/`: basegame managed/content outputs.
- `build/<preset>/shared/`: shared contract builds only.
- `build/<preset>/tools/`: repo-wide tool builds.
- `build/<preset>/deps/`: preset-specific dependency build and stamp outputs.
- `build/dependencies/`: shared third-party source/download caches.

Core managed outputs belong under `build/<preset>/<owner>/managed/`; core managed intermediates belong under `build/<preset>/<owner>/managed-obj/`. Tool managed outputs belong under `build/<preset>/tools/<tool-project>/managed/`; tool intermediates belong under `build/<preset>/tools/<tool-project>/managed-obj/`. Native outputs belong under `build/<preset>/<owner>/native/bin/` and `build/<preset>/<owner>/native/lib/`.

Logs are generated and owner-partitioned:

- `logs/client/`
- `logs/server/`
- `logs/basegame/`
- `logs/shared/`
- `logs/build/`
- `logs/tools/`

Bundled server logs stay under `logs/server/` even when the server was launched by the graphical client for singleplayer.

Bundle composition rules:

- `octaryn_client_bundle` is the graphical client package. It includes validated client assets, shared contracts, basegame/module payloads, and a version-matched `server/` payload for local singleplayer worlds.
- `octaryn_server_bundle` is the dedicated headless package. It includes server authority, shared contracts, basegame/module payloads, and server-side validation without client presentation payloads.
- Server files copied into the client bundle must originate from server-owned outputs. Copying does not transfer ownership to client and must not become a monolithic client/server implementation target.
- Bundle validators must reject missing `server/` payloads in the client bundle and reject client rendering/window/audio/UI payloads in the dedicated server bundle.

## Threading And Work Scheduling

The active architecture uses one host-owned scheduling model. Computation and gameplay logic must be written so they can run safely through it.

Thread roles:

- Main thread: process startup/shutdown, platform event pumping, presentation handoff, final frame submission, and narrow API-required main-thread work. It must not own gameplay, chunk generation, simulation, asset processing, or bulk computation.
- Coordinator thread: frame/tick scheduling, dependency graph assembly, work submission, synchronization fences, cancellation, deterministic handoff, and barriers between client, server, basegame/module logic, and tools.
- Worker pool: actual computation. It starts with at least two workers and scales to available cores by host policy. It runs simulation systems, gameplay systems, world generation, mesh/data preparation, asset processing, validation jobs, async save/load prep, replication prep, and other CPU-heavy logic.

Scheduling rules:

- All computation systems and gameplay logic run as jobs in the worker pool or through approved host APIs backed by that pool.
- New systems declare read/write access, ordering dependencies, cancellation behavior, frame/tick ownership, and commit barriers before activation.
- Client presentation work may prepare data on workers, but graphics API calls, window events, and final presentation remain client-owned platform paths.
- Server authority runs through coordinator-scheduled jobs and commits through deterministic server tick barriers.
- Basegame and external modules do not own threads, tasks, timers, unmanaged worker loops, or private worker pools.
- Taskflow executes Octaryn schedules. Octaryn owns phases, read/write declarations, barriers, cancellation, deterministic ordering, and profiler ownership.

## Complete System Inventory

The research report's main planning gap was breadth: every load-bearing system needs a named owner, a shared source of truth, and a first validation milestone before implementation starts.

| System family | Primary owner | Shared source of truth | First planning milestone |
| --- | --- | --- | --- |
| Lifecycle, bootstrap, config, crash, logging, hostfxr bridge | client, server, tools | Owner manifests, ABI/hosting contracts | Owner startup and failure-path contract. |
| ECS declarations and gameplay systems | basegame, game modules/mods | Component/system descriptors and Arch/native bridge descriptors | Generated schema plus Arch/native execution bridge. |
| Scheduler, query execution, debug inspection | client, server, tools | Phase/read-write declarations, query contracts, inspection contracts | Host-owned scheduler/query execution and tooling probes. |
| Module/game/mod loading, manifests, trust, capabilities | client, server, tools | Manifest schema, capability IDs, dependency model | Activation gates and trust tiers. |
| World model, chunks, bounds, coordinates, 512-height split | server, shared | World bounds and coordinate contracts | Clean world constants model. |
| Blocks, entities, items, inventories, recipes, tags, game state | basegame, server | Registries, definitions, component descriptors | Content registry and delta formats. |
| Physics, world interaction, fluids, gases | server, client prediction where allowed | Physics declarations, queries, intents, event contracts | Jolt/voxel interaction abstraction. |
| Networking, replication, prediction, interest management | server, client | Command, snapshot, replication, compatibility descriptors | LiteNetLib/LiteEntitySystem host spine hidden by Octaryn contracts. |
| Persistence, migrations, saves, corruption handling | server, tools | Save schema declarations and migration contracts | Region/entity container and migrator API. |
| UI, input, localization, accessibility, world-space surfaces | client, basegame | UI model, action IDs, input maps, style/localization IDs | Retained UI model, focus graph, raycast routing. |
| Rendering, shaders, assets, animation, audio, tooling | client, tools | Asset, shader, material, animation, and audio event declarations | Cooked asset pipeline and presentation boundary. |

The most dangerous failure mode is not a missing library; it is a backend concept leaking into `octaryn-shared` or module code. ECS storage, LiteNetLib sessions, LiteEntitySystem objects, Jolt bodies, Yoga nodes, SDL GPU resources, OpenAL handles, filesystem paths, and raw schedulers must stay behind owner APIs.

## Core Host Baseline

The core host should boot into a simple inspectable world before any game module adds richer behavior:

- flying camera only
- no default player physics
- no default collision controller
- no default survival/avatar rules
- no product main menu or game UI
- flat blank terrain
- 512-block world height
- vertical world span centered around origin
- deterministic owner-routed build/log output

This keeps the host useful for debugging and rendering while preventing core host behavior from becoming hidden game policy.

Basegame, game modules, or mods may add:

- player movement rules
- physics bodies
- entity controllers
- world generation
- interaction rules
- inventory/items
- UI overlays
- main menu and front-end flow
- progression/game state

Those additions must still go through explicit APIs and host-owned ECS execution.

World constants should be explicit:

- chunk width/depth is not the same concept as world height
- world height target is 512
- coordinate mapping should support a centered vertical range
- server authority owns valid-world bounds
- client presentation consumes server/shared bounds instead of hardcoding them
- old `CHUNK_HEIGHT = 256` and chunk-edge-derived world height constants are temporary port debt, not the target architecture

## Client And Server Launch Modes

Octaryn has two user-facing launch modes and one authority model.

- Graphical client: `octaryn_client_bundle` is the playable local application. It owns windowing, input, rendering, audio, client UI, local prediction views, presentation, and the user flow for singleplayer or multiplayer.
- Client server app: `client_server_app` starts or attaches to a bundled server-owned local session from the client bundle's `server/` payload when creating or loading a singleplayer world.
- Dedicated server: `octaryn_server_bundle` is separately runnable as a headless terminal/server package. It owns the same authority path as the bundled server and has no client rendering, audio, UI, windowing, or GPU dependency.
- Multiplayer client: remote multiplayer uses the same command, snapshot, tick, replication, disconnect, and error contracts as singleplayer. Singleplayer must not use privileged client-only mutation paths.

Singleplayer readiness flow:

1. Client selects create/load singleplayer world and requested game modules.
2. Client starts or attaches to the bundled server session through a client-owned launch/supervision API.
3. Server validates manifests, compatibility, capabilities, content/assets, package allowlists, and multiplayer/singleplayer compatibility.
4. Server creates or opens the world save, initializes world state, activates basegame/modules, and starts server ticks.
5. Server publishes a local endpoint/session handle and a ready snapshot.
6. Client connects through shared command/snapshot contracts, receives initial state, and enters world presentation.
7. Return-to-menu, quit, crash, or world-close flows stop the bundled server through server-owned shutdown APIs and wait for save-close completion where required.

Launch validation:

- Keep dedicated server launch probes separate from `client_server_app` launch probes.
- Keep the `client_server_app` readiness probe focused on the bundled server app startup path and expand it toward ready snapshot publication, clean disconnect, and save-close behavior as those runtime contracts land.
- Add bundle validation for required `server/` payload in the client bundle and absence of client presentation payload in the dedicated server bundle.

## Focused Master Plan Files

The master plan is split into focused files to keep each policy surface reviewable. These files are part of the same canonical plan:

- `docs/architecture/octaryn-master-plan-api.md`: API layers, developer tool APIs, physics, networking, and native backend policy.
- `docs/architecture/octaryn-master-plan-gameplay.md`: game module tiers, entity/block/item/fluid/gas/UI/game-state policy, replication, and persistence.
- `docs/architecture/octaryn-master-plan-build-and-validation.md`: source migration map, CMake/platform policy, target inventory, capability model, dependency decisions, and validation requirements.
- `docs/architecture/octaryn-master-plan-roadmap.md`: old-architecture port queue, performance invariants, lighting hold, near-term work, open decisions, and hard-no list.
