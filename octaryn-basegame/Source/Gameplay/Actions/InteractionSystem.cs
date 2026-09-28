using Octaryn.Basegame.Gameplay.Items;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Actions;

// Eye-ray interaction: keeps the look target fresh in the ECS world and
// converts use actions into gameplay effects. World items in reach are
// targeted by ray-sphere intersection and picked up exactly; the targeted
// item is replicated so clients can highlight it. Audio and UI are optional
// host backends; the system records interactions regardless.
public sealed class InteractionSystem
{
    public const float ReachDistance = 6.0f;
    public const string UseActionPrefix = "interact.use";

    // Generous targeting radius: dropped items render larger than their
    // physics capsule, and aiming at small pickups must stay forgiving.
    private const float ItemTargetRadius = 0.45f;

    private readonly IHostPhysicsApi? _physics;
    private readonly IHostAudioApi? _audio;
    private readonly IHostReplicationApi? _replication;
    private readonly IHostDiagnosticsApi? _diagnostics;
    private readonly ItemSystem _items;
    private ulong _publishedItemTarget;
    private uint _publishedItemId;
    private uint _publishedItemCount;

    public InteractionSystem(
        IHostPhysicsApi? physics,
        IHostAudioApi? audio,
        IHostReplicationApi? replication,
        IHostDiagnosticsApi? diagnostics,
        ItemSystem items)
    {
        _physics = physics;
        _audio = audio;
        _replication = replication;
        _diagnostics = diagnostics;
        _items = items;
    }

    public void Tick(IHostEcsApi ecs)
    {
        if (!PlayerEntity.TryGet(ecs, out var player) ||
            !ecs.TryGetComponent(player, out PlayerBodyComponent body) ||
            !body.Initialized)
        {
            return;
        }

        var cosPitch = MathF.Cos(body.Pitch);
        var directionX = cosPitch * MathF.Sin(body.Yaw);
        var directionY = MathF.Sin(body.Pitch);
        var directionZ = -cosPitch * MathF.Cos(body.Yaw);

        var worldDistance = ReachDistance;
        var hitSomething = false;
        HostRaycastHit hit = default;
        if (_physics is not null)
        {
            hitSomething = _physics.Raycast(
                body.EyeX, body.EyeY, body.EyeZ,
                directionX, directionY, directionZ,
                ReachDistance, out hit);
            if (hitSomething)
            {
                worldDistance = hit.Distance;
            }
        }

        FindItemTarget(ecs, body, directionX, directionY, directionZ,
            worldDistance, out var itemEntity, out var itemId, out var itemCount,
            out var itemDistance);

        ecs.SetComponent(player, new LookTargetComponent
        {
            HasTarget = hitSomething,
            MaterialId = hitSomething ? hit.MaterialId : 0u,
            Distance = hitSomething ? hit.Distance : -1.0f,
            PointX = hit.PointX,
            PointY = hit.PointY,
            PointZ = hit.PointZ,
            HasItemTarget = itemEntity != 0,
            ItemEntityId = itemEntity,
            ItemId = itemId,
            ItemCount = itemCount,
            ItemDistance = itemDistance,
        });
        PublishItemTarget(itemEntity, itemId, itemCount);
    }

    // Action router callback: "interact.use" on the current look target. A
    // targeted item is collected exactly; a world surface plays its feedback.
    public bool HandleAction(IHostEcsApi ecs, string actionId)
    {
        if (actionId != UseActionPrefix || !PlayerEntity.TryGet(ecs, out var player))
        {
            return false;
        }

        if (!ecs.TryGetComponent(player, out PlayerBodyComponent body) || !body.Initialized)
        {
            return false;
        }

        // Input may have changed the authoritative view since the last world tick.
        Tick(ecs);
        if (!ecs.TryGetComponent(player, out LookTargetComponent target))
        {
            return false;
        }

        if (target.HasItemTarget)
        {
            var collected = _items.TryCollectEntity(ecs, target.ItemEntityId);
            _diagnostics?.Write(
                HostLogLevel.Debug,
                $"octaryn.basegame interact_item entity={target.ItemEntityId} id={target.ItemId} collected={collected}");
            return collected;
        }

        if (!target.HasTarget)
        {
            return false;
        }

        _audio?.PlayActionSound(HostActionSoundIds.Select, 1.0f, target.PointX, target.PointY, target.PointZ);
        _diagnostics?.Write(
            HostLogLevel.Debug,
            $"octaryn.basegame interact material={target.MaterialId} distance={target.Distance:F2}");
        return true;
    }

    // Nearest world item whose sphere crosses the eye ray, never behind a
    // world surface hit. Analytic ray-sphere: cheap for the drop counts in
    // flight and exact enough for pickup targeting.
    private void FindItemTarget(
        IHostEcsApi ecs, PlayerBodyComponent body,
        float directionX, float directionY, float directionZ, float maxDistance,
        out ulong itemEntity, out uint itemId, out uint itemCount, out float itemDistance)
    {
        var bestT = maxDistance;
        ulong bestEntity = 0;
        uint bestId = 0;
        uint bestCount = 0;
        _items.QueryNearby(ecs, body.EyeX, body.EyeY, body.EyeZ, maxDistance + ItemTargetRadius,
            (ModuleEntity item, ref WorldItemComponent dropped) =>
        {
            var dx = dropped.X - body.EyeX;
            var dy = dropped.Y - body.EyeY;
            var dz = dropped.Z - body.EyeZ;
            var t = dx * directionX + dy * directionY + dz * directionZ;
            if (t < 0.0f || t >= bestT)
            {
                return;
            }

            var closestX = dx - t * directionX;
            var closestY = dy - t * directionY;
            var closestZ = dz - t * directionZ;
            if (closestX * closestX + closestY * closestY + closestZ * closestZ >
                ItemTargetRadius * ItemTargetRadius)
            {
                return;
            }

            bestT = t;
            bestEntity = item.Id;
            bestId = dropped.ItemId;
            bestCount = dropped.Count;
        });
        itemEntity = bestEntity;
        itemId = bestId;
        itemCount = bestCount;
        itemDistance = bestEntity != 0 ? bestT : -1.0f;
    }

    // Replicated on change only: clients highlight while the event stream
    // holds the same target and clear on a zero id.
    private void PublishItemTarget(ulong itemEntity, uint itemId, uint itemCount)
    {
        if (itemEntity == _publishedItemTarget && itemId == _publishedItemId &&
            itemCount == _publishedItemCount)
        {
            return;
        }

        if (_replication?.AvailableChangeCapacity == 0) return;
        _publishedItemTarget = itemEntity;
        _publishedItemId = itemId;
        _publishedItemCount = itemCount;
        if (itemEntity != 0)
        {
            _diagnostics?.Write(
                HostLogLevel.Debug,
                $"octaryn.basegame interact_target_item entity={itemEntity} id={itemId} count={itemCount}");
        }
        _replication?.PublishChange(
            BasegameReplicationEvent.ItemTargeted, itemEntity, itemId,
            itemEntity != 0 ? itemCount : 0u);
    }
}
