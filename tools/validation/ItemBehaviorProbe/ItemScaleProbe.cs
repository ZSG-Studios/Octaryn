using System.Diagnostics;
using System.Text.Json;
using Octaryn.Basegame.Gameplay.Inventory;
using Octaryn.Basegame.Gameplay.Items;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Server.Host;
using Octaryn.Shared.Host.Api;

internal static class ItemScaleProbe
{
    public static void Run()
    {
        foreach (var count in new[] { 100, 1000, 10000 })
        foreach (var awakePercent in new[] { 0, 1, 100 })
        {
            using var ecs = new ArchHostEcsApi();
            var player = PlayerEntity.Create(ecs);
            ecs.SetComponent(player, new PlayerBodyComponent { Initialized = true });
            var items = new ItemSystem(null, null, null, new InventorySystem(null));
            for (var i = 0; i < count; ++i)
            {
                var entity = items.Spawn(ecs, ItemCatalog.Apple, 1, 100 + i % 100 * 8, 100, i / 100 * 8, 0, 0, 0);
                ecs.TryGetComponent(entity, out WorldItemComponent item);
                item.Grounded = i >= count * awakePercent / 100;
                ecs.SetComponent(entity, item);
            }
            for (var frame = 0; frame < 20; ++frame) items.Tick(ecs, 1.0 / 60);
            var allocations = GC.GetAllocatedBytesForCurrentThread();
            var times = new double[180];
            for (var frame = 0; frame < times.Length; ++frame)
            {
                var start = Stopwatch.GetTimestamp();
                items.Tick(ecs, 1.0 / 60);
                times[frame] = Stopwatch.GetElapsedTime(start).TotalMilliseconds;
            }
            allocations = GC.GetAllocatedBytesForCurrentThread() - allocations;
            var near = 0;
            items.QueryNearby(ecs, 0, 0, 0, 6,
                (ModuleEntity _, ref WorldItemComponent _) => ++near);
            var retained = 0;
            ecs.Query<WorldItemComponent>((ModuleEntity _, ref WorldItemComponent _) => ++retained);
            if (near != 0 || retained != count) throw new InvalidOperationException("Spatial index lost or misclassified distant items");
            Array.Sort(times);
            Console.WriteLine("item_scale=" + JsonSerializer.Serialize(new {
                workload = "module_spatial_analytic_fall_no_box3d", count, awakePercent,
                p50_ms = times[90], p95_ms = times[171], p99_ms = times[178],
                worst_ms = times[^1], allocations_bytes_per_tick = allocations / times.Length,
                distant_candidates = near, retained
            }));
        }
    }
}
