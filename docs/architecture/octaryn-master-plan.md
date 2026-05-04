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
- `Source/Native/`: C/C++ implementation and native owner bridge code.
- `Source/Managed/`: C# implementation for that owner.
- `Source/Libraries/`: small owner-local native libraries before promotion to clearer domain folders.
- `Assets/`: runtime assets owned by that owner.
- `Shaders/`: shader source owned by that owner.
- `Tools/`: tools specific to that owner.
- `Data/`: structured content/config/data specific to that owner.

Shared `Assets/`, `Shaders/`, `Data/`, `Tools/`, and `Source/Libraries/` stay empty unless a pure implementation-free shared need is approved. Server `Shaders/` stays empty unless a real server-owned compute/offline shader need appears. Basegame may own `Assets/`, `Shaders/`, `Data/`, and `Tools/` because it is the bundled content/game module.

Owner landing zones:

```text
octaryn-client/
  CMakeLists.txt
  Octaryn.Client.csproj
  Source/
    Native/
    Libraries/
    Managed/
    ClientHost/
    Input/
    Audio/
    Ui/
    WorldPresentation/
    Rendering/
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
    Native/
    Libraries/
    Managed/
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
    Native/
    Libraries/
    Managed/
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
    Managed/
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

## API Layers

### Content API

Used to define things:

- blocks
- items
- entities
- fluids
- gases
- recipes
- tags
- loot
- biomes/features
- UI contribution declarations
- global game-state records

Content definitions are declarations. They do not own storage or host internals.

### Component API

Everything that can carry state should be component-backed:

- block components, such as hardness, drops, replaceability, fluid/gas behavior, collision, tool requirements
- item components, such as stack size, durability, use action, container, fuel, equipment slot
- entity components, such as transform, health, movement, inventory, AI state, physics body, ownership
- UI components, such as visible panel state, selected slot, screen model, notification state
- game-state components, such as day state, world rules, team state, progression flags

Components declare:

- stable component ID
- owner module ID
- data shape
- default value
- serialization schema
- replication policy
- persistence policy
- authority policy
- allowed phases and systems

Component data model rules:

- Components are schema-visible value layouts with stable IDs and module namespaces.
- Components explicitly opt into replication, persistence, prediction, presentation-only storage, or local-only storage.
- Entity runtime handles and persistent IDs are separate concepts.
- Block state should be compact definition ID plus compact state schema; sparse block entities exist only when needed.
- Item stacks are definition ID plus count and bounded payload sections; items are not mini-entities by default.
- UI state is retained node/model state plus action IDs, not ad hoc widget instances.
- Native and Arch-managed layouts must be checked by descriptor/schema validators before data crosses owner boundaries.

### System API

Modules write behavior as scheduled systems. Systems declare:

- system ID
- owner module ID
- phase
- server/client/prediction/presentation scope
- component reads
- component writes
- event reads/writes
- command outputs
- ordering dependencies
- whether the system is deterministic
- whether it is multiplayer compatible

Modules do not create threads, tasks, schedulers, sockets, file watchers, native loops, or worker pools.

System execution rules:

- Arch.System may define managed gameplay systems, but those systems are driven by Octaryn host phases and declared read/write sets.
- Native systems are host-owned or trusted ABI-gated systems only. They must declare the same phase, read/write, ordering, capability, and determinism metadata as managed systems.
- Structural changes are deferred through host command buffers and committed at explicit barriers.
- Server systems own authority. Client systems own prediction, interpolation, UI, and presentation unless a shared contract says otherwise.
- Hidden mutable global state is not allowed in module systems.

### Command And Query API

Modules can request work through explicit commands and queries:

- spawn/despawn entity
- set block
- use item
- apply damage
- emit interaction event
- query nearby entities
- query world blocks through approved views
- query inventory through approved views
- request UI action through client-owned UI capability

Commands are intent, not authority. The server validates and applies authoritative commands.

## Developer Tool APIs

Modules should have elegant, high-power tools, but those tools must be Octaryn-owned APIs with capability checks. The rule is:

> Expose power through Octaryn APIs, not implementation access.

Approved tool-style APIs can include:

- `Octaryn.Math`: vectors, matrices, quaternions, transforms, bounds, rays, colors, curves, interpolation, easing, coordinate transforms, and stable value types shared across C# and native ABI.
- `Octaryn.Geometry`: AABB, OBB, spheres, capsules, frustums, planes, ray intersections, shape intersections, shape casts, broadphase query descriptors, and spatial helpers.
- `Octaryn.Random`: deterministic RNG streams for gameplay, world generation, loot, particles, procedural content, and replay-safe simulation.
- `Octaryn.Time`: tick IDs, fixed-step time, cooldowns, timers, rates, durations, interpolation fractions, and server/client time mapping.
- `Octaryn.Collections`: approved fixed buffers, stable IDs, handles, readonly views, compact sets, and safe query result containers.
- `Octaryn.Diagnostics`: module diagnostics, counters, traces, markers, and structured logs routed through host-owned diagnostics.
- `Octaryn.Serialization`: schema/version helpers for declared component data, save migration, and network serialization descriptors without arbitrary file access.
- `Octaryn.Noise`: approved deterministic noise helpers for content/worldgen declarations when allowed by capability.
- `Octaryn.Localization`: localization IDs, format arguments, plural/gender metadata, and text lookup handles without direct file access.
- `Octaryn.Permissions`: role and permission declarations for admin commands, server operations, and gated module actions.

These APIs should feel excellent for C# authors, but they must stay stable, small, explicit, and backend-independent. Shared may define value types and contracts; optimized C++ implementations may back them behind host bridges when needed.

Tool APIs must not expose:

- native pointers
- renderer handles
- transport sessions
- raw ECS storage
- raw physics world
- process or filesystem access
- direct threading or scheduling objects
- broad service locators

## Physics API

Physics is required, but it must be exposed as declarations, commands, events, and queries rather than raw physics backend access.

Module-facing declarations:

```csharp
entity.PhysicsBody(body =>
{
    body.Dynamic();
    body.Capsule(height: 1.8f, radius: 0.35f);
    body.Mass(80.0f);
    body.Friction(0.8f);
    body.Restitution(0.0f);
    body.CollisionLayer("octaryn.basegame.layer.player");
});
```

Allowed physics API shapes:

- body declarations
- shape declarations
- collision layer declarations
- trigger/sensor declarations
- material/friction/restitution declarations
- constraints and joints when capability-approved
- ray casts
- shape casts
- overlap queries
- contact events
- trigger events
- movement/force/impulse intents
- character movement intent APIs
- prediction-safe client physics queries when explicitly declared

Server owns authoritative physics execution. Client may run prediction and presentation copies through client-owned APIs. Modules can read query results and emit commands, but cannot step the physics world or access raw backend state.

Example system:

```csharp
public sealed class PlayerMovementSystem : GameSystem
{
    public override void Build(SystemBuilder system)
    {
        system.ServerOnly();
        system.Reads<PlayerMoveInput>();
        system.Writes<PhysicsVelocity>();
        system.Uses<PhysicsQueries>();
    }

    public override void Tick(SystemContext context)
    {
        foreach (var player in context.Query<PlayerMoveInput, PhysicsVelocity>())
        {
            var ground = context.Physics.CastDown(player.Entity, 0.2f);
            player.Velocity = Movement.Apply(player.Input, player.Velocity, ground);
        }
    }
}
```

Physics hard boundaries:

- no raw Jolt or backend world access
- no unmanaged callbacks into modules
- no direct simulation step control
- no direct body pointer access
- no private collision broadphase access
- no client authority over server physics
- no physics package references in modules; module physics access is through Octaryn declarations, queries, events, and intents only

Planned backend policy:

- Jolt is the first physics backend candidate for host-owned rigid bodies, character support, collision queries, rollback-oriented save/restore hooks, and multicore execution.
- PhysX is deferred and may be revisited only under a user-approved physics plan if Jolt does not meet a concrete requirement.
- Custom Octaryn voxel collision and world-interaction kernels sit beside the physics backend for block/world interaction, fluids, gases, and dense voxel queries.
- Query results used by authoritative logic must be canonicalized by Octaryn APIs so backend query-order differences cannot leak into deterministic simulation.

The backend is a host implementation detail. Modules see declarations, queries, events, and intents only.

Physics validation milestones:

- Physics declaration validator: shapes, layers, materials, body modes, and query permissions are legal for the requested capabilities.
- Query determinism validator: authoritative query results are sorted/canonicalized before gameplay logic sees them.
- Physics launch/probe path: server can create, step, query, and destroy a small physics world without exposing backend handles.
- Prediction boundary probe: client prediction can run a permitted presentation/prediction copy without gaining authority over server physics.

## Networking API

Networking should also be powerful without exposing transport internals.

High-level declarations:

```csharp
component<Health>().Replicated();
component<Transform>().Replicated(EntityReplicationMode.Interpolated);
command<PlayerJump>().ClientToServer().Reliable();
event<FootstepEvent>().ServerToNearbyClients().Unreliable();
```

Lower-level API-owned declarations may be allowed:

```csharp
network.Message<PlayerAbilityCommand>()
    .Direction(NetworkDirection.ClientToServer)
    .Reliability(NetworkReliability.ReliableOrdered)
    .Authority(NetworkAuthority.ServerValidated)
    .Serializer<PlayerAbilityCommandSerializer>();
```

Allowed networking tools:

- custom command declarations
- custom event declarations
- replicated component declarations
- interest/visibility declarations
- serializer declarations through approved interfaces
- reliability/channel declarations through Octaryn enums
- server/client direction declarations
- prediction and reconciliation policy declarations

Networking hard boundaries:

- no raw sockets
- no LiteNetLib/LiteEntitySystem types in shared/module public APIs
- no transport session access
- no encryption/session internals
- no arbitrary packet loops
- no client-authoritative state mutation unless the API marks it prediction-only and server validated

Transport implementation belongs in client/server owners. Shared defines shapes and policies only.

Replication and transport policy:

- LiteNetLib is the hidden transport layer for client/server owners.
- LiteEntitySystem is a kept hidden client/server implementation component where it fits; Octaryn descriptors and protocol stay authoritative.
- Octaryn owns the public gameplay networking contracts: connection handshake, module hash/capability negotiation, command frames, snapshots, chunk/block deltas, entity/component replication declarations, UI action messages, disconnect reasons, and compatibility errors.
- LiteNetLib and LiteEntitySystem types must not appear in shared/module public APIs. Basegame, game modules, and mods use Octaryn contracts only.

Networking validation milestones:

- Handshake probe: client/server exchange API version, game/module list, content hashes, capabilities, and compatibility flags.
- Replication descriptor validator: replicated component IDs, ownership, interpolation/prediction policy, and any hidden LiteEntitySystem mapping are stable.
- Command authority validator: client-to-server commands are intent and server-validated, never direct authoritative mutation.
- Interest management probe: entity/chunk relevance decisions are deterministic and auditable by server logs.
- Disconnect/error contract validator: failures produce shared error shapes that client UI can present without seeing transport internals.

## Native C++ Backend

The C++ side should own high-throughput systems that must be fast and host-controlled:

- native ECS storage and archetype/chunk iteration
- scheduler/job execution and dependency graph
- block/world-space interaction kernels
- block edit commit pipeline
- fluids and gases
- core entity/world collision and contact queries
- core entity transform and spatial indexing
- chunk/block storage and streaming
- serialization and save packing
- replication snapshot packing and dirty tracking
- interest management
- native command buffers
- native event queues
- deterministic simulation kernels

C# or other module-facing code can define logic and declarations, but backend execution should compile/bridge into C++ owner systems where speed or authority matters.

Native plugin systems may exist later, but only through a narrow ABI and manifest-declared capabilities. Native modules still must not receive raw host internals.

Arch ECS is a first-class managed ECS layer for gameplay declarations, basegame systems, game modules, mods, and approved owner-local managed worlds. Native owner ECS/storage should be added where the host needs explicit C++ control: dense world/chunk state, authoritative server kernels, prediction/presentation mirrors, replication packing, persistence packing, fluids/gases, and other high-throughput paths.

The native side should use archetype/chunk storage where it is built, with stable component IDs, generation-counted entity handles, descriptor-validated layouts, deferred structural mutation, and separate authoritative, prediction, presentation, and UI world views where needed. Arch-managed worlds and native owner storage must meet through explicit Octaryn descriptors, bridges, and validators rather than direct raw storage access.

Native and managed ECS bridge rules:

- Arch remains the managed authoring and gameplay ECS layer.
- Native owner storage is added only for clear host needs: dense world data, authoritative kernels, presentation mirrors, networking/persistence packers, fluids/gases, and similar high-throughput paths.
- Shared descriptors define the bridge: component ID, field schema, serializer identity, replication policy, persistence policy, authority policy, and version.
- No module receives native storage pointers or Arch world internals owned by another host.
- Bridge failures are validation failures, not runtime fallback paths.

## Game Module Tiers

Modules should be handled as trust tiers, not as one binary trusted/untrusted switch.

| Tier | Who it covers | Allowed power |
| --- | --- | --- |
| Bundled first-party modules | `octaryn-basegame` and official game modules shipped with the product. | Managed Arch ECS systems, content, UI, assets, save schemas, and host-provided native kernels when explicitly declared. |
| Signed trusted extensions | Future trusted partners or first-party extensions. | Managed API model like bundled modules. External native code stays unavailable until the native-code policy is deliberately selected. |
| Managed external gameplay mods | Normal external mods. | Managed code against `octaryn-shared`, approved packages, approved framework API groups, explicit capabilities, and no native/thread/filesystem/network/process access. |
| Data-only content packs | Packs with data/assets only. | Content, assets, localization, tags, recipes, loot, UI themes where capability-approved; no code execution. |

### Tier 1: Declarative Content

Safest and fastest. Module declares data only.

Examples:

```csharp
block.Component<Hardness>(new(3.0f));
item.Component<StackSize>(new(64));
entity.Component<Health>(new(20, 20));
```

### Tier 2: Approved Managed Systems

Module logic compiled against `octaryn-shared` only.

```csharp
public sealed class FurnaceSystem : GameSystem
{
    public override void Build(SystemBuilder system)
    {
        system.ServerOnly();
        system.Phase(GamePhase.Simulation);
        system.Reads<FurnaceFuel>();
        system.Writes<FurnaceProgress>();
    }

    public override void Tick(SystemContext context)
    {
        // Uses explicit Octaryn APIs only.
    }
}
```

### Tier 3: Owner-Provided Native Systems

For performance-critical systems owned by client/server/basegame hosts. External native module code is not active until the native-code policy is deliberately selected.

```cpp
extern "C" OctarynSystemDescriptor octaryn_register_systems(OctarynApi* api);
```

Native systems receive only approved handles:

- component registry
- query builder
- command buffer
- event writer
- diagnostics
- Octaryn allocation scopes if explicitly allowed
- Octaryn scheduled-work scopes if explicitly allowed

They do not receive raw ECS storage, renderer access, sockets, filesystem access, process access, backend allocator/job/scheduler types, or private host state.

Native systems are not the default mod model. Prefer host-provided native kernels selected by stable capability/kernel ID over arbitrary native module code.

## Entity API

Entities should be easy to define while still mapping to ECS/backend networking.

```csharp
public sealed class Zombie : EntityDefinition
{
    public override void Build(EntityBuilder entity)
    {
        entity.Component<Transform>();
        entity.Component<Health>(new(20, 20));
        entity.Component<PhysicsBody>();
        entity.Component<ZombieAiState>();

        entity.Networked();
        entity.Persistent();
        entity.ServerLogic<ZombieAiSystem>();
        entity.ClientPresentation<ZombiePresentation>();
    }
}
```

The host turns that into:

- native component layout
- spawn/despawn contract
- replication schema
- persistence schema
- authority rules
- server systems
- client presentation/prediction systems
- validation rules

Entity logic should never directly serialize network packets or touch transport internals.

## Blocks, Items, Fluids, And Gases

Blocks and items should also be ECS-backed definitions, not one-off hardcoded systems.

```csharp
public sealed class CopperOreBlock : BlockDefinition
{
    public override void Build(BlockBuilder block)
    {
        block.Component<Hardness>(new(3.0f));
        block.Component<Drops>("octaryn.basegame.item.raw_copper");
        block.Component<Mineable>(ToolKind.Pickaxe);
        block.OnBreak<CopperOreBreakRule>();
        block.Persistent();
        block.Networked();
    }
}
```

High-throughput world materials belong in native C++ pipelines:

- liquids
- gases
- falling blocks
- spreading blocks
- block-to-entity interactions
- entity-to-world contact behavior
- block update queues
- world-space queries

Basegame/modules define rules and components. Server native systems execute authoritative simulation. Client native systems render/predict/present results.

World simulation rules:

- Server owns authoritative block edits, fluid/gas simulation, falling/spreading blocks, block entity ticks, and entity-to-world contact behavior.
- Basegame/modules declare material behavior, components, rules, and systems through shared APIs.
- Fluid and gas simulation must be active-region and budgeted; it must not become an unbounded world tick.
- Cross-chunk queues must be deterministic and committed at server tick barriers.
- Client may predict or present fluids/gases only through server-compatible snapshot/prediction contracts.
- DDGI/skylight behavior stays out of this plan until the separate lighting plan exists.

## UI And Input

Product UI is game-owned. The main menu, pause menu, inventory screens, HUD, world selection flow, character screens, and game-specific options should come from `octaryn-basegame` or another active game module through explicit UI contribution APIs.

UI must support both screen-space and world-space surfaces as first-class presentation targets. The same declarative UI model should be able to render as:

- screen-space UI
- world-space UI
- render-to-texture UI
- textured 3D quads/panels
- attached block/entity panels
- floating labels/nameplates
- hologram or diegetic panels
- HUD overlays

The common flow is:

```text
module UI declaration
-> client UI model
-> screen-space surface or offscreen texture
-> optional world-space quad/panel
-> client input routing, focus, and raycast projection
```

Core/client UI should stay limited to debug/developer surfaces:

- launch probes
- diagnostics overlays
- profiler panels
- rendering/debug views
- validation/dev tools
- emergency host error screens

The client host owns the actual window, input routing, renderer, focus, and UI execution. Modules declare UI models and actions; they do not receive renderer/window/input internals.

Modules may declare:

- actions
- keybind/action IDs
- main menu screens
- pause menu screens
- inventory screen models
- HUD slots
- panels
- screen-space surfaces
- world-space surfaces
- render-to-texture surfaces
- world-space anchors
- block/entity attachment targets
- raycast input behavior
- focus behavior
- style/theme tokens
- localization keys
- accessibility metadata
- item/entity/block inspector rows
- client-side presentation systems

Example API shape:

```csharp
ui.Surface("octaryn.basegame.furnace.screen")
    .ScreenSpace()
    .Modal()
    .Input(UiInputMode.Focus);

ui.Surface("octaryn.basegame.furnace.world_panel")
    .WorldSpace()
    .Size(1.2f, 0.7f)
    .AttachToBlock()
    .RenderToTexture()
    .Input(UiInputMode.Raycast);
```

World-space UI rendering should be implemented by the client renderer as scene geometry. A typical implementation renders 2D UI into a texture, places that texture on a 3D quad, and maps pointer/controller/raycast hits back into UI coordinates.

Final UI stack direction:

- Octaryn owns a retained UI tree, diff/invalidation, action routing, focus graph, controller navigation, localization hooks, style/theme tokens, and world-space surface mapping.
- Yoga is the planned hidden layout solver. It must not become a module-facing document or widget API.
- SDL3_ttf is the text rendering layer, including its FreeType/HarfBuzz-backed shaping and fallback path. It is not a product UI framework by itself. Do not add separate first-wave HarfBuzz planning unless text validation proves it necessary.
- RmlUi is deferred unless a user-approved UI authoring plan chooses a markup/CSS-style layer behind Octaryn UI contracts.
- ImGui and related widgets stay debug/tool/editor surfaces, not basegame product UI.

UI validation milestones:

- UI declaration validator: surfaces, anchors, actions, focus modes, style/theme IDs, and localization IDs are declared and capability-approved.
- Focus/raycast probe: screen-space and world-space UI map input to the same action model.
- Render-to-texture probe: a retained UI surface can render into a texture and be placed on a 3D quad without exposing GPU handles to modules.
- Product/debug split validator: basegame product UI stays module-owned; core/client UI stays debug, diagnostics, profiler, validation, editor, or emergency host UI.

Modules may not:

- own the window
- read raw keyboard/mouse/controller state directly
- call renderer APIs
- create UI threads
- access native renderer resources
- directly allocate GPU textures
- directly submit draw calls
- directly manage font/glyph atlases
- bypass client-owned focus/input routing

Flow:

1. Module declares `ActionId`.
2. Client maps physical input to action intent.
3. Client sends command intent if authority is needed.
4. Server validates and applies.
5. Client presents predicted or replicated result.

## Game State

Game state is ECS-backed singleton/global component data with explicit ownership.

Examples:

- world rules
- day/time progression
- difficulty
- teams
- quest/progression state
- weather state
- server event state

Server owns authoritative game state. Client receives snapshots or presentation views. Modules declare state shape, save schema, replication policy, and migration rules.

## Replication And Persistence

Modules declare intent:

```csharp
component<Health>()
    .Replicated()
    .Persistent();

component<Transform>()
    .Replicated(EntityReplicationMode.Interpolated)
    .PredictedForOwner();
```

The backend handles:

- stable IDs
- dirty tracking
- snapshot packing
- delta compression
- interest filtering
- save/load
- schema migration
- prediction/rollback later
- multiplayer compatibility checks

Modules do not manually open sockets or serialize transport packets.

Persistence policy:

- Hot world/chunk/entity/inventory data uses custom binary sectioned containers with explicit versions, checksums, journals, module namespaces, and migration metadata.
- JSON remains for manifests, inspectable metadata, and content/tooling records, preferably through Glaze on native paths.
- LZ4 is the default for hot chunk/cache compression where decode speed matters.
- Zstd is the default for colder save snapshots, backups, package transfer, and bulk migration outputs.
- FlatBuffers can be evaluated for selected control-plane envelopes if a schema tool is useful. It should not own the dense voxel/chunk save format.
- Save compatibility must be explicit: a module declares schema versions and migrations before its persisted data can be trusted.

Save container requirements:

- Section table with versioned section IDs.
- Module namespace and content hash metadata.
- Checksums for corruption detection.
- Journal or transactional write path for crash recovery.
- Explicit migration records and replayable migration tools.
- Separate hot world/chunk/ECS data from inspectable metadata.
- Owner-routed logs for load, save, migration, corruption, and recovery paths.

## Source-To-Destination Migration Map

Every port slice starts with an old-source inventory and an explicit destination or removal reason.

Client source candidates:

- `old-architecture/source/app/` window/application startup pieces -> `octaryn-client/Source/Native/` or `octaryn-client/Source/Managed/` by exact concern.
- `old-architecture/source/rendering/`, `old-architecture/source/gpu/`, render upload paths, shader pipeline setup -> `octaryn-client/Source/Rendering/`, `octaryn-client/Shaders/`, or focused client native libraries.
- Client-side input, camera, display, audio, overlays, debug UI -> `octaryn-client/Source/Input/`, `Source/Audio/`, `Source/Ui/`, `Source/WorldPresentation/`, or client debug/tool surfaces.
- Client-side mesh planning, presentation snapshots, and upload descriptors -> `octaryn-client/Source/WorldPresentation/`.
- Exclude DDGI, skylight propagation, and lighting rewrites until the dedicated lighting plan exists.

Server source candidates:

- `old-architecture/source/world/edit/` -> `octaryn-server/Source/World/Blocks/` or `World/Queries/`.
- Server-side pieces of `old-architecture/source/world/runtime/`, `world/chunks/`, `world/jobs/`, and `world/generation/` -> `octaryn-server/Source/World/`, `Simulation/`, or `Tick/`.
- `old-architecture/source/physics/` -> `octaryn-server/Source/Physics/` for authority and client prediction wrappers only where explicitly planned.
- Server-owned persistence pieces from `old-architecture/source/core/persistence/` -> `octaryn-server/Source/Persistence/`.
- Exclude DDGI, skylight propagation, lighting architecture, and old CPU skylight behavior until the dedicated lighting plan exists.

Basegame source candidates:

- High-level content definitions from `old-architecture/source/world/block/` -> `octaryn-basegame/Data/`, `Source/Content/`, or generator-only metadata after stripping storage, lighting, mesh, and old host-state details.
- Texture atlas and content import behavior serving basegame content -> `octaryn-basegame/Tools/`.
- Player rules, interaction rules, item/block rules, recipes, tags, loot, features, biome rules, and product UI -> `octaryn-basegame/Source/Gameplay/`, `Source/Content/`, `Source/Ui/`, `Data/`, or `Assets/`.
- Existing `skylightOpacity` values may stay as basegame block metadata, but must not become DDGI, skylight propagation, or lighting host contracts until the lighting plan exists.

Shared source candidates:

- Pure IDs, value types, world bounds, positions, directions, block/entity references, command/snapshot records, host API IDs, capability IDs, manifest records, validation result shapes, and ABI layout/version records -> `octaryn-shared/Source/`.
- Shared must not receive implementation, scanners, asset processors, persistence engines, transport code, renderer code, or gameplay policy.

Tools and CMake candidates:

- Old build helpers, old CMake modules, old desktop helper tools, and old profiling wrappers stay under `old-architecture/` until intentionally ported.
- Old atlas builder -> `octaryn-basegame/Tools/` because it is basegame-specific content tooling.
- Repo-wide validators, profiling wrappers, build orchestration, package checks, shader tooling, and developer operations -> root `tools/`.
- Old RenderDoc helpers stay in `old-architecture/`; RenderDoc is an external developer tool.

## CMake And Platform Architecture

Root `cmake/` is split by responsibility:

- `cmake/Shared/`: repo-wide CMake defaults, warning policy, output layout, build/log owner paths, shared helper functions, and naming rules.
- `cmake/Owners/`: target construction for client, server, shared contracts, basegame assets/modules, and tools. Owner modules may call shared helpers and dependency aliases but must not contain platform detection.
- `cmake/Dependencies/`: dependency wrappers and allowed dependency aliases grouped by real owner need, not a global old dependency bag.
- `cmake/Platforms/`: host platform facts and distro-family policy.
- `cmake/Toolchains/`: compiler, sysroot, target triple, find-root behavior, and platform knobs only. Toolchain files must not create Octaryn targets or fetch dependencies.

Platform rules:

- Active configure presets are exactly `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`.
- Linux-hosted builds are Clang-only in active lanes.
- Windows targets are cross-built from Linux/Arch using `cmake/Toolchains/Windows/clang.cmake`.
- LLVM MinGW is an implementation detail of the Windows toolchain, not a public platform folder or preset name.
- Active Podman build wrappers use the Linux-hosted toolchain environment for Linux and Windows targets; expand those wrappers in place instead of adding host-specific build layouts.
- Linux distro policy is split by family only when real package/tool behavior differs. Current planned families are Arch, Debian, Fedora, and Suse/openSUSE.
- Platform modules report capabilities. Owner targets decide whether to use them. Platform modules must not own gameplay, rendering, server, basegame, or sandbox behavior.

Old CMake port map:

- `old-architecture/cmake/BuildLayout.cmake` -> `cmake/Shared/OwnerBuildLayout.cmake`, after renaming away from old product names and enforcing owner build/log paths.
- Old dependency cache paths like `build/shared/deps/<bucket>` and `logs/deps/<bucket>` -> `build/dependencies/` plus preset-specific `build/<preset>/deps/`.
- `old-architecture/cmake/ProjectOptions.cmake` -> `cmake/Shared/ProjectDefaults.cmake` plus owner-specific options.
- `old-architecture/cmake/Dependencies.cmake` -> `cmake/Dependencies/`, split by policy, aliases, and owner groups.
- `old-architecture/cmake/CPM.cmake` -> `cmake/Dependencies/` only if CPM remains selected.
- `old-architecture/cmake/toolchains/windows-x64.cmake` -> `cmake/Toolchains/Windows/clang.cmake`; GCC MinGW is not an active lane.
- `old-architecture/CMakePresets.json` -> root presets only after the owner/platform/toolchain split exists.
- Old build scripts -> root `tools/build/` only after they select new owner presets and write only to approved build/log paths.

## Build Target Inventory

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

Internal dependency targets such as `octaryn_dotnet_hosting` and `octaryn_native_threads` are implementation details for owner CMake modules. They are not public build targets and must stay under dependency/owner modules rather than old-architecture target wiring.

Planned focused targets are added only when implementation exists:

```text
octaryn_client_assets
octaryn_basegame_assets
octaryn_module_validator
octaryn_module_sandbox_contracts
```

## Capability Model

Capabilities are deny-by-default and explicit.

Examples:

- `content.blocks`
- `content.items`
- `content.entities`
- `content.override`
- `content.ui`
- `gameplay.systems`
- `gameplay.interactions`
- `world.queries.read`
- `world.blocks.edit.intent`
- `entities.spawn.intent`
- `inventory.mutate.intent`
- `ui.contribute`
- `input.actions`
- `math.core`
- `geometry.queries`
- `random.deterministic`
- `time.tick`
- `diagnostics.module`
- `physics.declare`
- `physics.query`
- `physics.intent`
- `network.replicated_components`
- `network.messages`
- `persistence.components`
- `native.systems`
- `world.fluids.read`
- `world.fluids.edit.intent`
- `world.gases.read`
- `world.gases.edit.intent`
- `worldgen.biomes`
- `worldgen.features`
- `worldgen.noise`
- `content.recipes`
- `content.loot`
- `content.tags`
- `content.assets`
- `content.localization`
- `ui.theme`
- `ui.worldspace`
- `audio.emit`
- `animation.contribute`
- `save.schemas`
- `save.migrations`
- `multiplayer.compat`
- `mod.dependencies`
- `asset.hashes`
- `admin.commands`
- `permissions.roles`
- `probes.readonly`
- `editor.tools`

Capabilities should be specific enough that a module can be approved for item definitions without gaining entity spawning, filesystem access, networking, renderer access, or native code.

Capability families:

| Family | Examples |
| --- | --- |
| Content | `content.blocks`, `content.items`, `content.entities`, `content.recipes`, `content.loot`, `content.tags`, `content.assets`, `content.localization` |
| Gameplay | `gameplay.systems`, `gameplay.interactions`, `inventory.mutate.intent`, `entities.spawn.intent` |
| World | `world.queries.read`, `world.blocks.edit.intent`, `world.fluids.read`, `world.fluids.edit.intent`, `world.gases.read`, `world.gases.edit.intent`, `worldgen.biomes`, `worldgen.features`, `worldgen.noise` |
| Presentation | `ui.contribute`, `ui.theme`, `ui.worldspace`, `input.actions`, `audio.emit`, `animation.contribute` |
| Host contracts | `physics.declare`, `physics.query`, `physics.intent`, `network.replicated_components`, `network.messages`, `persistence.components` |
| Compatibility | `save.schemas`, `save.migrations`, `multiplayer.compat`, `asset.hashes`, `mod.dependencies` |
| Operations | `diagnostics.module`, `admin.commands`, `permissions.roles`, `probes.readonly`, `editor.tools`, `native.systems` |

Capabilities grant only Octaryn API access. They never grant raw backend access.

## Dependency Decisions

| Dependency or candidate | Decision | Public API rule |
| --- | --- | --- |
| Arch, Arch.System, Arch.EventBus, Arch.Relationships | Keep as managed gameplay/module ECS stack. | Allowed only through approved package policy; Arch stays module/owner-local and no Arch types may appear in `octaryn-shared` public contracts. |
| Taskflow | Keep as hidden job execution substrate. | Modules see Octaryn scheduling contracts only. |
| LiteNetLib | Keep as hidden transport backend. | Modules see Octaryn networking contracts only. |
| LiteEntitySystem | Keep as hidden client/server implementation component where it fits. | Octaryn replication descriptors, protocol, authority, and public contracts remain canonical; no LiteEntitySystem entity/RPC/SyncVar concepts leak into shared/module APIs. |
| Jolt | First physics backend candidate. | Modules see Octaryn physics declarations, queries, events, and intents only. |
| PhysX | Deferred candidate only by user-approved physics plan. | No active dependency or API surface. |
| Yoga | First layout solver candidate. | Modules see Octaryn UI declarations only. |
| RmlUi | Deferred candidate only by user-approved UI authoring plan. | No active dependency or API surface. |
| SDL3 GPU, SDL3_ttf | Keep in client presentation/text paths. | No raw SDL window, renderer, GPU, font, or event handles in module APIs. |
| ImGui stack | Debug/tool/editor UI only. | Not basegame product UI. |
| OpenAL Soft | Favored hidden runtime audio backend. | Modules declare audio events; no backend handles. |
| miniaudio | Helper/decode/streaming/tool roles unless benchmarks choose otherwise. | No backend handles in module APIs. |
| Glaze | JSON metadata/manifests/tooling. | Shared contracts stay BCL/value-shape only unless a contract-only dependency is approved. |
| LZ4 | Hot chunk/cache compression. | Server/tools implementation detail. |
| Zstd | Cold saves, backups, bulk transfer. | Server/tools implementation detail. |
| FlatBuffers | Optional control-plane envelope candidate. | Not dense voxel/chunk/entity save format. |
| Protobuf, Cap'n Proto | Not primary world save formats. | Do not build the save model around them. |
| Taffy | Deferred. | Rust/FFI/toolchain complexity; reconsider only if Yoga cannot satisfy layout needs under a user-approved UI layout plan. |
| fastgltf, KTX, meshoptimizer, ozz-animation, shaderc, SPIR-V tools, SPIRV-Cross/Shadercross | Keep for assets/shaders/animation tooling and client presentation as appropriate. | Modules declare assets/materials/animations through Octaryn contracts. |
| Recast/Detour | Later navigation candidate. | Defer until world, physics, persistence, and tooling spines are stable. |
| EnTT, Flecs, Nuklear | Not planned public-core choices. | Do not introduce as module-facing architecture. |

### Managed Package And Framework Allowlist

`octaryn-shared` stays package-free or BCL-only unless a contract-only dependency is deliberately approved here. `Directory.Packages.props` pins versions; it is not permission for a module or project to reference a package.

| Package or API group | Allowed owners | Purpose | Runtime scope | Validation rule |
| --- | --- | --- | --- | --- |
| `Arch` | `octaryn-basegame`, approved modules/mods, `octaryn-client`, `octaryn-server` | Managed ECS for gameplay, basegame, modules, mods, and approved owner-local worlds. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types; bridges meet native storage through Octaryn descriptors. |
| `Arch.LowLevel` | Transitive package for approved Arch runtime packages. | Low-level Arch runtime support. | Transitive module runtime only. | May not be referenced directly unless promoted to an explicit approved package. |
| `Arch.System` | `octaryn-basegame`, approved modules/mods, `octaryn-client`, `octaryn-server` | Managed ECS system authoring/execution support driven by Octaryn host scheduling declarations. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types; systems declare Octaryn phases and read/write sets. |
| `Arch.System.SourceGenerator` | Projects that directly define Arch systems. | Compile-time ECS system generation. | Build/analyzer only. | Must be requested as build package with private analyzer assets only. |
| `Arch.EventBus` | `octaryn-basegame`, approved modules/mods, owner-local client/server systems. | Gameplay and host integration events. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types. |
| `Arch.Relationships` | `octaryn-basegame`, approved modules/mods, owner-local client/server systems. | Gameplay and host entity relationships. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types. |
| `Collections.Pooled` | Transitive package for approved Arch runtime packages. | Pooled collection implementation used by Arch. | Transitive module runtime only. | No direct module reference unless explicitly promoted. |
| `CommunityToolkit.HighPerformance` | Transitive package for approved Arch runtime packages. | High-performance memory/collection primitives used by Arch. | Transitive module runtime only. | No direct module reference unless explicitly promoted. |
| `Microsoft.Extensions.ObjectPool` | Transitive package for approved Arch runtime packages. | Object pooling used by Arch package graph. | Transitive module runtime only. | No direct module reference unless explicitly promoted. |
| `ZeroAllocJobScheduler` | Transitive package for approved Arch runtime packages. | Scheduler support used by Arch.System. | Transitive module runtime only. | No direct module reference; source and binary checks deny `Schedulers.*` access. |
| Roslyn/source-generator transitive packages | Build graph only. | Analyzer/source-generator support. | Build/analyzer only. | Must be reachable from approved build packages only. |
| Safe BCL value APIs | shared, basegame, approved modules/mods. | Primitives, collections, spans/memory, math/numerics, dates/times, text, and diagnostics abstractions. | Shared contracts and module implementation. | Allowed namespace/API group only; no filesystem, network, reflection, process, environment, threading, or direct console writes. |
| Host-routed JSON/data parsing | basegame, approved modules/mods, tools. | Content parsing through approved host APIs. | Offline tools or bounded host API runtime path. | Module cannot open arbitrary paths; host supplies bounded streams/handles. |
| `LiteNetLib` | `octaryn-client`, `octaryn-server`. | Reliable UDP transport. | Host transport only. | Rejected in shared, basegame, modules, and mods. |
| `LiteEntitySystem` | `octaryn-client`, `octaryn-server`. | Host-side entity replication/synchronization backend. | Host implementation only. | Rejected in shared, basegame, modules, and mods; public entities/RPCs/SyncVars/replication declarations remain Octaryn API shapes. |

Denied to modules by default: `System.IO`, raw filesystem paths, `System.Net`, sockets, HTTP clients, `System.Diagnostics.Process`, unmanaged interop, unsafe native bridges, reflection/dynamic loading, runtime code generation, arbitrary threading/task scheduling, timers, custom worker pools, environment variables, direct host service discovery, direct console/stdout/stderr writes, and unlisted NuGet packages.

### Native Support Targets And Wrappers

Old native targets map to focused support or owner targets:

| Old target | Destination |
| --- | --- |
| `octaryn_engine_log` | `octaryn_native_logging`, used by client, server, tools, and focused support libraries. |
| `octaryn_engine_diagnostics` | `octaryn_native_diagnostics`, used by executables and tools that need crash reports. |
| `octaryn_engine_memory` | `octaryn_native_memory`; SDL coupling removed while porting. |
| `octaryn_engine_imgui_backend` | Client debug/tool UI only. |
| `octaryn_engine_shader_tool` | Root `tools/` shader compiler; generated shaders are client-owned assets. |
| `octaryn_engine_texture_atlas` | `octaryn-basegame/Tools/`; generated atlas assets are consumed by client. |
| `octaryn_managed_game` | `octaryn-basegame`. |
| `octaryn_engine_shader_assets` | `octaryn-client` asset build. |
| `octaryn_engine_runtime_assets` | `octaryn-client` packaging. |
| `octaryn_engine_runtime_bundle` | Replaced by `octaryn_client_bundle`; old name only in migration notes. |
| `octaryn_engine_runtime` | Split across client, server, shared, basegame, tools, and focused support targets. Do not recreate it. |

Native dependency wrappers:

| Wrapper | Allowed owners |
| --- | --- |
| `octaryn::deps::spdlog` | Native logging support, client, server, tools. |
| `octaryn::deps::cpptrace` | Native diagnostics support and executable crash paths. |
| `octaryn::deps::tracy` | Client, server, tools, and profiling-enabled support. |
| `octaryn::deps::mimalloc` | Native memory support only; consumers use the support target. |
| `octaryn::deps::taskflow` | Native jobs support below Octaryn scheduler policy; modules never see Taskflow. |
| `octaryn::deps::unordered_dense` | Owner-local implementation code that needs dense hash containers. |
| `octaryn::deps::eigen` | Shared pure math/value code or owner-local math; rendering-only math stays client-owned. |
| `octaryn::deps::glaze` | JSON metadata/manifests/tooling; shared contracts only if pure value-shape need is approved. |
| `octaryn::deps::sdl3` | Client only, except isolated tools that truly need SDL. |
| `octaryn::deps::sdl3_image` | Client UI/assets and asset import tools. |
| `octaryn::deps::sdl3_ttf` | Client UI and overlays only; not product UI framework. |
| ImGui-related wrappers | Client debug UI and tools only. |
| Shader tooling wrappers | Shader tooling only. |
| Asset import wrappers | Asset tools; client only for intentional runtime loading/optimization. |
| `octaryn::deps::openal` | Planned hidden spatial runtime audio backend under client. |
| `octaryn::deps::miniaudio` | Client audio helpers and tools unless benchmarks consolidate differently. |
| `octaryn::deps::zlib`, `lz4`, `zstd` | Server persistence, asset tools, and tooling caches as appropriate. |

## Validation Requirements

Before a module can activate, validate:

- manifest identity and version
- requested capabilities
- requested host APIs
- content declarations
- asset declarations
- component declarations
- system declarations
- read/write sets
- phase ownership
- package allowlist
- framework API allowlist
- native ABI if present
- no direct threading/task creation
- no direct filesystem/process/network/reflection/native interop; approved capabilities grant bounded Octaryn handles only
- replication and persistence schema compatibility
- multiplayer compatibility
- trust tier and signature/hash policy
- save schema and migration compatibility
- asset hashes and cooked payload consistency
- component schema ABI/layout identity
- replication schema compatibility
- world bounds invariants
- deterministic scheduler ordering
- save corruption recovery path
- performance budgets for chunk meshing, replication packing, save writes, and fluid/gas simulation

Validation contracts live in `octaryn-shared`. Validator implementation lives in `tools`. Activation gates and execution live in client/server hosts.

Validator backlog from the research report:

| Validator/probe | Enforces |
| --- | --- |
| Managed IL sandbox scan | No P/Invoke, denied framework namespaces, raw threading/timers/process/filesystem/network APIs, or dynamic loading. |
| Component schema ABI check | Arch/generated descriptors and native descriptors agree on field layout, version, serializer identity, authority, replication, and persistence policy. |
| Replication compatibility probe | Component replication annotations and network IDs remain stable or migrate explicitly. |
| Save migration replay validator | Old saves can open, migrate, resave, and preserve declared compatibility. |
| Content ID collision validator | Block, item, entity, tag, recipe, loot, UI, and asset IDs do not collide across modules. |
| Asset hash and manifest validator | Package manifests match cooked payloads, hashes, declared assets, and multiplayer compatibility metadata. |
| Shader reflection/material ABI validator | Cooked shaders match client material and render-contract expectations. |
| World bounds invariant validator | 512-height world model stays independent from chunk width/depth constants. |
| Deterministic scheduling probe | Read/write declarations, phase graph, barriers, and ordering produce stable results. |
| Save corruption recovery probe | Journals, checksums, backups, and recovery paths work under direct runtime checks. |
| Performance budget probes | Chunk meshing, replication packing, save writes, fluid/gas simulation, and UI layout stay within declared thresholds. |
| Trust/signature validator | Signed package policy, server-required mod list, content hashes, and client negotiation are coherent. |

If a failure can cause silent corruption, undefined privilege, broken multiplayer, or broken save compatibility, it deserves a validator before the feature is treated as complete.

Targeted validation paths for plan and structure work:

- `git diff --check -- README.md AGENTS.md docs .github`
- grep docs for stale `smoke`, `ctest`, old product naming, generic bucket names, and retired research wording.
- `python3 tools/validation/validate_cmake_target_inventory.py --build-dir build/debug-linux/cmake` when a configured build tree exists.
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_policy_separation`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_dependency_aliases`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_project_references`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_package_policy_sync`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_scheduler_contract`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_abi_contracts`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_owner_boundaries`

If a plan change claims current bundle/module behavior, also use the matching targeted owner validation:

- `octaryn_validate_module_manifest_probe`
- `octaryn_validate_module_layout`
- `octaryn_validate_basegame_block_catalog`
- `octaryn_validate_server_world_generation_probe`
- `octaryn_validate_client_server_app`
- `octaryn_validate_bundle_module_payload`

Do not use smoke tests or `ctest` as validation paths unless the user explicitly asks.

## Old-Architecture Port Gap Queue

The old architecture remains the source inventory for behavior parity, but only non-lighting slices may move before a dedicated lighting plan exists. Current validated coverage includes basegame block/content catalogs, basegame atlas payload metadata and client-side atlas tile sampling, basegame worldgen rules, server terrain generation and spawn-column seeding, server block edits/persistence overrides, world time persistence, client snapshot presentation, and graphical client launch probes that consume generated server terrain.

Remaining non-lighting gaps should be handled in the order below unless direct validation shows a blocker. This queue refines the near-term world, interaction, client/server launch, physics/presentation, and content-tooling phases; it does not reopen lighting work.

| Gap | Old source | Destination | Validation |
| --- | --- | --- | --- |
| Client bundle data and asset discovery | `old-architecture/source/core/asset_paths.*`, app startup asset reads | `octaryn-client/Source/Native/AssetPaths/` and client launch probes | Client app launch probe must read module `Data/` and `Assets/` from the bundle root, not workspace paths. |
| Real native atlas upload/render path | `old-architecture/source/render/atlas/`, `old-architecture/source/render/world/`, shader atlas use | `octaryn-client/Source/Native/Rendering/` and `octaryn-client/Shaders/` | Client launch validation now samples bundled color-atlas tiles and validation-only normal/specular material swatches through SDL texture rendering; full SDL_GPU 2D array upload, shader binding, mips, animation updates, and PBR validation remain open. |
| Player movement, collision, spawn, and camera state | `old-architecture/source/app/player/`, `old-architecture/source/core/camera/camera.*`, `old-architecture/source/physics/` | Basegame movement rules, server physics authority, client prediction/presentation | Focused player/physics probes plus client presentation checks; no client-owned authority. |
| Block raycast and interaction UX | `old-architecture/source/app/overlay/interaction.*`, `old-architecture/source/world/edit/` | Shared command/query contracts, basegame interaction rules, server block authority, client input routing | Interaction probe should cover raycast target selection, accepted/rejected edits, support rules, and snapshot replication. |
| Player, chunk, and world-save metadata | `old-architecture/source/core/persistence/player.cpp`, `chunk_files.cpp`, `world.cpp`, `world_save.cpp`, `world_snapshot.cpp` | `octaryn-server/Source/Persistence/` with shared save/version contracts only | Player save file round trips, current JSON chunk-column override files, and active world-save metadata summaries are server-owned coverage; compressed chunk cache semantics, save migration/replay probes, corruption handling, and compatibility IDs remain open. |
| Dynamic fluids and gases | `old-architecture/source/world/edit/water.cpp`, old material/block behavior, and future fluid rules | Basegame fluid/gas declarations and server active-region simulation | Fluid/gas probes must prove deterministic cross-chunk queues, tick budgets, snapshots, and client presentation handoff. |
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

Do not add DDGI, lighting probes, server lighting contracts, or client lighting rewrites from this ECS/API plan. Lighting files in `old-architecture/` are reference material only until that plan exists.

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
