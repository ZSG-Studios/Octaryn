using Octaryn.Basegame.Gameplay.Inventory;
using Octaryn.Basegame.Gameplay.Items;
using Octaryn.Basegame.Persistence;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame;

public sealed partial class GameContext
{
    private bool _restored;

    public void BeginSaveSession(uint sessionId) => _items.BeginSaveSession(sessionId);

    public int CaptureSaveState(Span<byte> destination)
    {
        if (_ecs is null || !_ecs.TryGetComponent(_playerEntity, out HotbarComponent hotbar))
            throw new InvalidOperationException("Basegame save requires an active player ECS.");
        var items = new List<(ulong Id, WorldItemComponent State)>();
        _ecs.Query<WorldItemComponent>((ModuleEntity entity, ref WorldItemComponent state) =>
        {
            if (items.Count == GameplaySnapshot.MaximumItems)
                throw new InvalidOperationException("Basegame save world-item limit exceeded.");
            items.Add((entity.Id, state));
        });
        items.Sort((left, right) => left.Id.CompareTo(right.Id));
        var written = new GameplaySnapshot(_items.ReplicationId, _ecs.EntityIdWatermark, hotbar, items).Write(destination);
        if (written > IGameModuleSaveState.MaximumBytes) throw new InvalidOperationException("Basegame save limit exceeded.");
        return written;
    }

    public void RestoreSaveState(ReadOnlySpan<byte> source)
    {
        if (_restored || _ecs is null || source.Length > IGameModuleSaveState.MaximumBytes)
            throw new InvalidOperationException("Basegame save can only be restored once into a fresh activation.");
        var snapshot = GameplaySnapshot.Read(source, _playerEntity.Id);
        _ecs.SetComponent(_playerEntity, snapshot.Hotbar);
        _items.RestoreReplicationId(snapshot.ReplicationId);
        foreach (var (id, state) in snapshot.Items) _items.RestoreItem(_ecs, id, state);
        _ecs.ReserveEntityIds(snapshot.EntityWatermark);
        _restored = true;
    }
}
