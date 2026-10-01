namespace Octaryn.Shared.Host.Api;

// Engine-owned entity handle. The underlying storage (Arch on the managed
// host, native tables on a native host) is never exposed to modules.
public readonly record struct ModuleEntity(ulong Id)
{
    public static readonly ModuleEntity None = new(0);

    public bool IsValid => Id != 0;
}

public delegate void EcsQueryCallback<T>(ModuleEntity entity, ref T component);

// Capability-gated ECS access for game modules. The engine owns storage,
// queries and scheduling; modules declare plain struct components and work
// them through this contract only.
public interface IHostEcsApi
{
    ModuleEntity CreateEntity();

    // Restores an unused module-local identity and advances future allocation.
    ModuleEntity RestoreEntity(ulong persistentId);

    ulong EntityIdWatermark { get; }

    // Prevents reuse of identities belonging to entities removed before a save.
    void ReserveEntityIds(ulong watermark);

    void DestroyEntity(ModuleEntity entity);

    bool HasComponent<T>(ModuleEntity entity) where T : struct;

    bool TryGetComponent<T>(ModuleEntity entity, out T value) where T : struct;

    // Adds the component when missing, replaces it otherwise.
    void SetComponent<T>(ModuleEntity entity, in T value) where T : struct;

    // Visits every entity holding component T with mutable access.
    void Query<T>(EcsQueryCallback<T> callback);
}
