using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Player;

// Creates and resolves the module-owned player entity through the host ECS
// API. Multiplayer keeps one body per connected player; the query shape
// already supports that growth.
public static class PlayerEntity
{
    public static ModuleEntity Create(IHostEcsApi ecs)
    {
        var entity = ecs.CreateEntity();
        ecs.SetComponent(entity, new EntityTag());
        ecs.SetComponent(entity, new PlayerBodyComponent());
        ecs.SetComponent(entity, new Time.WorldClockComponent());
        ecs.SetComponent(entity, new Inventory.HotbarComponent());
        ecs.SetComponent(entity, new Actions.LookTargetComponent());
        return entity;
    }

    public static bool TryGet(IHostEcsApi ecs, out ModuleEntity entity)
    {
        var found = ModuleEntity.None;
        ecs.Query<EntityTag>((ModuleEntity candidate, ref EntityTag _) =>
        {
            if (!found.IsValid)
            {
                found = candidate;
            }
        });
        entity = found;
        return found.IsValid;
    }
}
