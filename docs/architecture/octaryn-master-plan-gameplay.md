# Octaryn Master Plan: Gameplay And UI

This file is part of the canonical `octaryn-master-plan.md` policy set. It owns gameplay/module tiers, content/runtime state, UI, replication, and persistence policy.

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

