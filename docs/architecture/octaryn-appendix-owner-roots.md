# Octaryn Appendix: Owner Roots

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
    Host/
      Program.cs
      Host.cs
    HostBridge/
    Libraries/
    Modules/
      Bundled/
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
