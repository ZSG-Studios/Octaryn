# Octaryn AAA Research Systems A

## Systems That Must Be Planned

Do not skip these. Add any missing systems you identify.

### 1. Platform, Host, And Process Lifecycle

Research:

- process startup/shutdown
- owner host bootstrap
- client app lifecycle
- server lifecycle
- tool lifecycle
- config loading
- command line
- environment policy
- crash handling
- logging
- diagnostics
- hot reload boundaries if any
- developer debug mode
- release mode
- module activation order
- owner bundle discovery
- hostfxr/native bridge startup
- ABI version checks
- failure-path reporting

Hard boundary:

- no generic runtime bucket
- no `Engine` namespace
- host internals are not module APIs

### 2. ECS Backend

Research:

- native C++ ECS backend vs managed Arch frontend split
- archetype storage
- chunked storage
- sparse sets
- entity IDs
- component IDs
- stable handles
- generation counters
- structural changes
- deferred command buffers
- deterministic iteration
- query caching
- read/write access declarations
- system graph scheduling
- server authority world
- client presentation world
- prediction ECS world
- replicated component storage
- persistent component storage
- component schema registration
- ABI-safe component data
- C# declaration to C++ storage mapping
- reflection-free registration
- code generation options
- binary component layout validation
- debug inspection
- profiler markers

Compare:

- custom native ECS
- EnTT
- Flecs
- Arch-only managed ECS
- hybrid Arch declarations with native execution/storage

Do not expose raw ECS storage to modules.

### 3. Scheduler And Threading

Research:

- one main thread
- one coordinator thread
- worker pool with at least two workers and scalable core count
- task graph scheduling
- tick phases
- frame phases
- deterministic barriers
- cancellation
- resource read/write tracking
- no hidden global mutable state
- no module-created threads
- no `Task.Run` from modules
- no private timers or schedulers from modules
- server tick commits
- client presentation handoff
- tool/offline jobs
- Tracy validation and profiling

Current planned native scheduler dependency:

- `taskflow v4.0.0`

Research whether Taskflow is enough or if a custom thin scheduler layer is needed above it.

### 4. Module, Game, And Mod Loading

Research:

- game module manifest schema
- mod manifest schema
- module API versioning
- dependency declarations
- load order
- content registration order
- capability requests
- package allowlists
- framework API allowlists
- binary inspection
- source validation
- asset declaration validation
- multiplayer compatibility declaration
- save compatibility declaration
- module signing/trust options
- server-required mod negotiation
- client-only mod policy
- server-only module policy
- basegame as bundled module
- external games using same path
- no broad reflection/service discovery
- no arbitrary NuGet dependencies

### 5. Entity System

Research:

- entity definition API
- components
- systems
- lifecycle
- spawn/despawn intent
- authority ownership
- transform
- hierarchy/relationships
- physics body attachment
- inventory attachment
- health/damage/status effects
- AI
- movement
- animation state
- persistence
- replication
- prediction
- interpolation
- interest management
- ownership transfer
- custom module components
- custom module systems
- native systems for high-performance logic
- script/system ordering
- events
- commands
- queries
- debugging and inspection

Target authoring style should be very easy:

```csharp
entities.Define("octaryn.basegame.entity.player")
    .Component<Transform>()
    .Component<Health>()
    .Component<PlayerInput>()
    .Component<PhysicsBody>()
    .Replicate<Transform>(ReplicationMode.Interpolated)
    .Persist<Health>();
```

The host should compile declarations into backend storage, networking, persistence, and validation.

### 6. Blocks And World Interaction

Research:

- block IDs
- block definitions
- block states
- block components
- block tags
- block collision
- block hardness
- block drops
- block interaction
- block ticking
- random ticks
- block entities/tile entities if needed
- block replacement
- block fluids/gases interaction
- server authority for edits
- client presentation and mesh generation
- world queries
- chunk snapshots
- neighborhood snapshots
- packed mesh plans
- atlas/material presentation data
- save format
- replication format
- content override/extension rules
- migration from old architecture

Core terrain baseline is flat blank terrain. Basegame adds real content and rules.

### 7. Items, Inventory, Equipment, And Recipes

Research:

- item IDs
- item definitions
- item components
- stack rules
- durability
- use actions
- cooldowns
- tags
- tools
- containers
- inventory APIs
- equipment slots
- crafting
- recipes
- fuel
- loot
- drops
- creative/dev inventories
- server authority
- UI bindings
- persistence
- replication
- mod extension and replacement rules

### 8. UI System

Research:

- retained-mode vs immediate-mode module UI API
- declarative UI model
- data binding
- actions
- commands
- focus
- input routing
- screen-space layout
- world-space layout
- render-to-texture
- 2D UI on 3D quads
- text rendering
- font fallback
- text shaping
- localization
- accessibility
- controller navigation
- keyboard/mouse capture
- drag/drop
- tooltips
- animation/transitions
- styling/themes
- invalidation/diffing
- UI state as ECS components
- UI model persistence where needed
- client-owned rendering
- server-validated UI actions
- debug UI vs product UI split

Evaluate whether SDL3_ttf is enough and whether Octaryn needs additional text/layout libraries such as HarfBuzz, FreeType policy, Yoga/Taffy-style layout, or a custom layout engine. Any dependency recommendation must preserve owner boundaries.

### 9. Input And Action Mapping

Research:

- raw input collection
- action maps
- contexts
- rebinding
- gamepad/controller support
- keyboard/mouse
- text input
- UI focus
- world-space UI pointer/raycast routing
- input commands
- prediction inputs
- server validation
- replay/determinism
- basegame action declarations
- mod action contributions
- accessibility bindings

### 10. Game State

Research:

- global game state ECS components
- world rules
- difficulty/progression
- teams/factions
- time/day state
- weather placeholder
- dimension/world state if needed
- game phase
- menu/front-end state
- save migration
- replication
- authority
- module-owned game state
- conflict rules between mods

### 11. Physics

Research:

- physics backend choice
- C++ host integration
- collision layers
- rigid bodies
- character movement
- constraints/joints
- triggers/sensors
- ray casts
- shape casts
- overlap queries
- broadphase
- block/world collision
- fluid/gas interaction hooks
- entity/world interaction
- server authority
- client prediction
- interpolation
- determinism expectations
- rollback/reconciliation
- save/restore
- debug draw
- profiling

Compare:

- Jolt
- PhysX
- Bullet
- ReactPhysics3D
- custom voxel collision plus narrow physics library

Modules must only see Octaryn physics declarations, queries, events, and intents. No raw backend world access.

### 12. Networking And Replication

Research:

- LiteNetLib host-only transport role
- LiteEntitySystem host-only replication role
- message schema
- component replication declaration
- reliable/unreliable channels
- command frames
- snapshots
- delta compression
- interpolation
- prediction
- reconciliation
- rollback if needed
- interest management
- ownership
- authority handoff
- entity spawn/despawn replication
- block edit replication
- inventory replication
- UI action messages
- anti-cheat/validation
- server browser/session future hooks
- mod compatibility handshake
- content hash negotiation
- protocol versioning
- serialization
- packet budgets
- bandwidth profiling

Transport code belongs in client/server. Shared defines message contracts only. Modules do not see sockets, LiteNetLib types, or transport sessions.

### 13. Persistence And Save Format

Research:

- world save layout
- player save layout
- entity component persistence
- block/chunk persistence
- item/inventory persistence
- game state persistence
- module save namespaces
- schema versioning
- migrations
- compression choices
- transactional writes
- corruption handling
- backups
- async save jobs
- server ownership
- client cache boundaries
- content/module compatibility checks
- save open/close lifecycle
- deterministic serialization
- binary vs JSON vs hybrid formats

Current compression dependencies include zlib, lz4, and zstd. Research how each should be used or rejected.

### 14. Fluids, Liquids, Gases, And Environmental Simulation

Research:

- liquid blocks
- gases
- spread rules
- pressure rules
- falling/flowing rules
- source blocks
- interactions with entities
- interactions with blocks
- fire/smoke/steam if planned
- server authoritative simulation
- native C++ kernels
- chunk boundaries
- scheduling
- replication
- persistence
- client presentation
- mod-defined fluid/gas behavior
- validation of custom behavior
- performance budgets

Important: core world is blank/flat. Basegame defines real liquid/gas/content rules. Heavy simulation should run in C++ backend through explicit APIs.

