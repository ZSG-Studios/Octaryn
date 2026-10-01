using Arch.Core;
using EcsWorld = Arch.Core.World;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

// Host-owned ECS backend over Arch. Arch types never cross the module
// boundary; modules see only ModuleEntity handles and their own components.
internal sealed class ArchHostEcsApi : IHostEcsApi, IDisposable
{
    private readonly EcsWorld _world = EcsWorld.Create();
    private readonly Dictionary<ulong, Entity> _entities = new();
    private readonly Dictionary<Entity, ulong> _ids = new();
    private ulong _nextId = 1;
    private bool _disposed;
    private static class QueryCache<T>
    {
        internal static readonly QueryDescription Description = new QueryDescription().WithAll<T>();
    }

    public ModuleEntity CreateEntity()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (_nextId == ulong.MaxValue) throw new InvalidOperationException("Entity identity capacity is exhausted.");
        var id = _nextId++;
        var entity = _world.Create();
        _entities[id] = entity;
        _ids[entity] = id;
        return new ModuleEntity(id);
    }

    public ModuleEntity RestoreEntity(ulong persistentId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (persistentId == 0 || persistentId == ulong.MaxValue || _entities.ContainsKey(persistentId))
            throw new ArgumentException("Persistent entity identity is invalid or already allocated.", nameof(persistentId));
        var entity = _world.Create();
        _entities.Add(persistentId, entity);
        _ids.Add(entity, persistentId);
        _nextId = Math.Max(_nextId, persistentId + 1);
        return new ModuleEntity(persistentId);
    }

    public ulong EntityIdWatermark
    {
        get { ObjectDisposedException.ThrowIf(_disposed, this); return _nextId - 1; }
    }

    public void ReserveEntityIds(ulong watermark)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (watermark == ulong.MaxValue) throw new ArgumentException("Entity identity capacity is exhausted.", nameof(watermark));
        _nextId = Math.Max(_nextId, watermark + 1);
    }

    public void DestroyEntity(ModuleEntity entity)
    {
        if (_entities.Remove(entity.Id, out var removed))
        {
            _ids.Remove(removed);
            _world.Destroy(removed);
        }
    }

    public bool HasComponent<T>(ModuleEntity entity) where T : struct
    {
        return _entities.TryGetValue(entity.Id, out var mapped) && _world.Has<T>(mapped);
    }

    public bool TryGetComponent<T>(ModuleEntity entity, out T value) where T : struct
    {
        if (_entities.TryGetValue(entity.Id, out var mapped) && _world.Has<T>(mapped))
        {
            value = _world.Get<T>(mapped);
            return true;
        }

        value = default;
        return false;
    }

    public void SetComponent<T>(ModuleEntity entity, in T value) where T : struct
    {
        var mapped = Map(entity);
        if (_world.Has<T>(mapped))
        {
            _world.Set(mapped, value);
        }
        else
        {
            _world.Add(mapped, value);
        }
    }

    public void Query<T>(EcsQueryCallback<T> callback)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var description = QueryCache<T>.Description;
        _world.Query(in description, (Entity entity, ref T component) =>
        {
            callback(new ModuleEntity(_ids[entity]), ref component);
        });
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        _entities.Clear();
        _ids.Clear();
        EcsWorld.Destroy(_world);
    }

    private Entity Map(ModuleEntity entity)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        return _entities.TryGetValue(entity.Id, out var mapped)
            ? mapped
            : throw new InvalidOperationException($"Unknown module entity {entity.Id}.");
    }
}
