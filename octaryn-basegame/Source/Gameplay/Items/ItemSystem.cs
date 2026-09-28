using Octaryn.Basegame.Gameplay.Inventory;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Items;

// World items: thrown drops fall and settle through the physics world, and
// pickups grant into the hotbar with exact count conservation. Every grant
// and drop is mirrored to the replication channel when one is attached.
public sealed class ItemSystem
{
    public const string DropAction = "inventory.drop";
    public const string DropStackAction = "inventory.drop_stack";

    private const float Gravity = 24.0f;
    private const float TossSpeed = 4.0f;
    private const float TossUp = 2.0f;
    private const float PickupRadius = 1.6f;
    // Tossed items become collectible once settled, or after the toss time on
    // hosts where physics cannot settle them.
    private const float PickupDelaySeconds = 1.0f;

    private readonly IHostPhysicsApi? _physics;
    private readonly IHostReplicationApi? _replication;
    private readonly IHostDiagnosticsApi? _diagnostics;
    private readonly InventorySystem _inventory;
    // Receipt/drop watermark channel: clients order grants and drops by this
    // strictly increasing id, so it must never repeat or regress.
    private ulong _nextReplicationId;
    private readonly ItemSpatialIndex _spatial = new();
    private readonly List<ulong> _awake = new();
    private readonly List<ulong> _nearby = new();

    public ItemSystem(
        IHostPhysicsApi? physics,
        IHostReplicationApi? replication,
        IHostDiagnosticsApi? diagnostics,
        InventorySystem inventory)
    {
        _physics = physics;
        _replication = replication;
        _diagnostics = diagnostics;
        _inventory = inventory;
    }

    public void Tick(IHostEcsApi ecs, double deltaSeconds)
    {
        var dt = (float)System.Math.Clamp(deltaSeconds, 0.0, 0.25);
        PlayerBodyComponent body = default;
        var hasPlayer = PlayerEntity.TryGet(ecs, out var player) &&
            ecs.TryGetComponent(player, out body) && body.Initialized;
        float playerX = 0.0f, playerY = 0.0f, playerZ = 0.0f;
        if (hasPlayer)
        {
            playerX = body.State.X;
            playerY = body.State.Y - 0.9f; // Body centre below the eye.
            playerZ = body.State.Z;
        }

        _spatial.Awake(_awake);
        foreach (var id in _awake)
        {
            var item = new ModuleEntity(id);
            if (!ecs.TryGetComponent(item, out WorldItemComponent dropped))
            {
                _spatial.Remove(item);
                continue;
            }
            dropped.Age += dt;
            if (!dropped.Grounded)
            {
                StepPhysics(ref dropped, dt);
                TraceAirborne(ref dropped, dt);
            }
            ecs.SetComponent(item, dropped);
            _spatial.Update(item, dropped);
            PublishPose(item, dropped);
        }
        if (!hasPlayer) return;
        QueryNearby(ecs, playerX, playerY, playerZ, PickupRadius,
            (ModuleEntity item, ref WorldItemComponent dropped) =>
            {
                if (!dropped.Grounded && dropped.Age < PickupDelaySeconds) return;
                var dx = dropped.X - playerX;
                var dy = dropped.Y - playerY;
                var dz = dropped.Z - playerZ;
                if (dx * dx + dy * dy + dz * dz <= PickupRadius * PickupRadius)
                    Collect(ecs, item, ref dropped);
            });
    }

    public void QueryNearby(IHostEcsApi ecs, float x, float y, float z, float radius,
        EcsQueryCallback<WorldItemComponent> callback)
    {
        _spatial.Nearby(x, y, z, radius, _nearby);
        foreach (var id in _nearby)
        {
            var item = new ModuleEntity(id);
            if (!ecs.TryGetComponent(item, out WorldItemComponent dropped))
            {
                _spatial.Remove(item);
                continue;
            }
            callback(item, ref dropped);
            if (ecs.HasComponent<WorldItemComponent>(item)) ecs.SetComponent(item, dropped);
        }
    }

    // Action router callback: drop one ("inventory.drop") or the whole
    // selected stack ("inventory.drop_stack") as a tossed world item.
    public bool HandleAction(IHostEcsApi ecs, string actionId)
    {
        var wholeStack = actionId == DropStackAction;
        if (!wholeStack && actionId != DropAction)
        {
            return false;
        }
        if (_replication?.AvailableChangeCapacity == 0 || _replication?.AvailableWorldItemCapacity == 0) return false;

        var removed = _inventory.TryRemoveSelected(ecs, wholeStack ? uint.MaxValue : 1u);
        if (removed.Count == 0)
        {
            return false;
        }

        float yaw = 0.0f;
        float pitch = 0.0f;
        float eyeX = 0.0f;
        float eyeY = 0.0f;
        float eyeZ = 0.0f;
        if (PlayerEntity.TryGet(ecs, out var player) &&
            ecs.TryGetComponent(player, out PlayerBodyComponent body) && body.Initialized)
        {
            yaw = body.Yaw;
            pitch = body.Pitch;
            eyeX = body.EyeX;
            eyeY = body.EyeY;
            eyeZ = body.EyeZ;
        }

        var cosPitch = MathF.Cos(pitch);
        Spawn(ecs, removed.ItemId, removed.Count,
            eyeX, eyeY - 0.4f, eyeZ,
            cosPitch * MathF.Sin(yaw) * TossSpeed,
            MathF.Sin(pitch) * TossSpeed + TossUp,
            -cosPitch * MathF.Cos(yaw) * TossSpeed);
        _replication?.PublishChange(BasegameReplicationEvent.ItemDropped, NextReplicationId(), removed.ItemId, removed.Count);
        _diagnostics?.Write(
            HostLogLevel.Debug,
            $"octaryn.basegame item_drop id={removed.ItemId} count={removed.Count}");
        return true;
    }

    public ModuleEntity Spawn(
        IHostEcsApi ecs, ushort itemId, uint count,
        float x, float y, float z, float velocityX, float velocityY, float velocityZ)
    {
        if (_replication?.AvailableWorldItemCapacity == 0)
            throw new InvalidOperationException("World item replication capacity exhausted before spawn.");
        var entity = ecs.CreateEntity();
        ecs.SetComponent(entity, new WorldItemComponent
        {
            ItemId = itemId,
            Count = count,
            X = x,
            Y = y,
            Z = z,
            VelocityX = velocityX,
            VelocityY = velocityY,
            VelocityZ = velocityZ,
            Grounded = false,
        });
        ecs.TryGetComponent(entity, out WorldItemComponent state);
        _spatial.Update(entity, state);
        PublishPose(entity, state);
        return entity;
    }

    private void PublishPose(ModuleEntity entity, in WorldItemComponent state, bool removed = false)
    {
        if (_replication is null) return;
        var pose = new HostWorldItemPose
        {
            EntityId = entity.Id, ItemId = state.ItemId, Count = state.Count,
            X = state.X, Y = state.Y, Z = state.Z,
            VelocityX = state.VelocityX, VelocityY = state.VelocityY, VelocityZ = state.VelocityZ,
            Flags = removed ? HostWorldItemPose.Removed : state.Grounded ? HostWorldItemPose.Sleeping : 0
        };
        if (!_replication.PublishWorldItem(pose))
            throw new InvalidOperationException("Authority rejected a valid world item pose.");
    }

    // Interact-use pickup of one specific world item (the look target), as
    // opposed to the radius vacuum in Tick. Count conservation is identical.
    public bool TryCollectEntity(IHostEcsApi ecs, ulong entityId)
    {
        var item = new ModuleEntity(entityId);
        if (!ecs.TryGetComponent(item, out WorldItemComponent dropped))
            return false;
        var collected = Collect(ecs, item, ref dropped);
        // Collect destroys a fully granted stack. A partial grant must write
        // the retained count back now that this is no longer a ref query.
        if (collected && ecs.HasComponent<WorldItemComponent>(item))
            ecs.SetComponent(item, dropped);
        return collected;
    }

    private ulong NextReplicationId()
    {
        return ++_nextReplicationId;
    }

    // Debug trace for items that refuse to settle (slopes, holes in the map).
    private void TraceAirborne(ref WorldItemComponent dropped, float dt)
    {
        dropped.TraceAccumulator += dt;
        if (dropped.TraceAccumulator < 1.0f)
        {
            return;
        }

        dropped.TraceAccumulator = 0.0f;
        _diagnostics?.Write(
            HostLogLevel.Debug,
            $"octaryn.basegame item_airborne id={dropped.ItemId} x={dropped.X:F2} y={dropped.Y:F2} z={dropped.Z:F2} v=({dropped.VelocityX:F2},{dropped.VelocityY:F2},{dropped.VelocityZ:F2})");
    }

    // Rigid-body fall through the host collision world: swept motion, bounce
    // with restitution, contact friction, and settle-to-sleep. Hosts without
    // a collision world keep the analytic fall so drops never freeze mid-air.
    private void StepPhysics(ref WorldItemComponent dropped, float dt)
    {
        if (_physics is not null)
        {
            var state = new HostWorldItemState
            {
                X = dropped.X,
                Y = dropped.Y,
                Z = dropped.Z,
                VelocityX = dropped.VelocityX,
                VelocityY = dropped.VelocityY,
                VelocityZ = dropped.VelocityZ,
                Grounded = dropped.Grounded,
                SleepTimer = dropped.SleepTimer,
            };
            if (_physics.StepWorldItem(ref state, dt))
            {
                dropped.X = state.X;
                dropped.Y = state.Y;
                dropped.Z = state.Z;
                dropped.VelocityX = state.VelocityX;
                dropped.VelocityY = state.VelocityY;
                dropped.VelocityZ = state.VelocityZ;
                dropped.SleepTimer = state.SleepTimer;
                if (state.Sleeping && !dropped.Grounded)
                {
                    _diagnostics?.Write(
                        HostLogLevel.Debug,
                        $"octaryn.basegame item_settle id={dropped.ItemId} count={dropped.Count} x={state.X:F2} y={state.Y:F2} z={state.Z:F2}");
                }
                dropped.Grounded = state.Sleeping;
                return;
            }
        }

        StepFallAnalytic(ref dropped, dt);
    }

    private void StepFallAnalytic(ref WorldItemComponent dropped, float dt)
    {
        dropped.VelocityY -= Gravity * dt;
        dropped.X += dropped.VelocityX * dt;
        dropped.Y += dropped.VelocityY * dt;
        dropped.Z += dropped.VelocityZ * dt;
        dropped.VelocityX *= 1.0f - 0.4f * dt;
        dropped.VelocityZ *= 1.0f - 0.4f * dt;
    }

    private bool Collect(IHostEcsApi ecs, ModuleEntity item, ref WorldItemComponent dropped)
    {
        if (_replication?.AvailableChangeCapacity == 0) return false;
        if (!ItemCatalog.TryGet(dropped.ItemId, out var entry))
        {
            return false;
        }

        var granted = _inventory.TryAdd(ecs, dropped.ItemId, dropped.Count, entry.MaxStack);
        if (granted == 0)
        {
            return false;
        }

        _replication?.PublishChange(BasegameReplicationEvent.ItemGranted, NextReplicationId(), dropped.ItemId, granted);
        _diagnostics?.Write(
            HostLogLevel.Debug,
            $"octaryn.basegame item_pickup id={dropped.ItemId} granted={granted} remaining={dropped.Count - granted}");
        if (granted >= dropped.Count)
        {
            PublishPose(item, dropped, removed: true);
            _spatial.Remove(item);
            ecs.DestroyEntity(item);
            return true;
        }

        // Partial grant: the world item keeps exactly what the hotbar refused.
        dropped.Count -= granted;
        PublishPose(item, dropped);
        return true;
    }
}
