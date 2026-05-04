# Octaryn Master Plan: API And Backends

This file is part of the canonical `octaryn-master-plan.md` policy set. It owns module-facing API layers and host backend boundaries.

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

