using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;

internal static class AuthorityItemProbe
{
    public static void Run(string bundle, int count)
    {
        bundle = Path.GetFullPath(bundle);
        const BindingFlags flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
        AssemblyLoadContext.Default.Resolving += (_, name) =>
            AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, name.Name + ".dll"));
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Server.dll"));
        var type = assembly.GetType("Octaryn.Server.Modules.ModuleActivator", true)!;
        using var module = (IDisposable)Activator.CreateInstance(type)!;
        var sinkType = assembly.GetType("Octaryn.Server.Host.ConsoleCommandSink", true)!;
        var sink = Activator.CreateInstance(sinkType)!;
        if ((int)type.GetMethod("Activate", flags)!.Invoke(module, [sink])! != 0)
            throw new InvalidOperationException("Actual authority activation failed");
        var policy = assembly.GetType("Octaryn.Server.Host.NativeHostPolicyLibrary", true)!;
        var startup = policy.GetMethod("CreateStartupFrame", flags)!.Invoke(null, null)!;
        var tick = type.GetMethod("Tick", flags)!;
        tick.Invoke(module, [startup]);
        var instance = type.GetField("_instance", flags)!.GetValue(module)!;
        var ecs = instance.GetType().GetField("_ecs", flags)!.GetValue(instance)!;
        var items = instance.GetType().GetField("_items", flags)!.GetValue(instance)!;
        // Diagnostics are disabled for this explicitly labelled throughput fixture.
        items.GetType().GetField("_diagnostics", flags)!.SetValue(items, null);
        var spawn = items.GetType().GetMethod("Spawn")!;
        var entities = new List<object>();
        for (int i = 0; i < count; ++i)
            entities.Add(spawn.Invoke(items, [ecs, (ushort)2, 1u,
                -10f + i % 100 * .2f, .6f, i / 100 * .1f, 0f, 0f, 0f])!);
        var snapshotType = startup.GetType();
        var input = snapshotType.GetField("Input")!.GetValue(startup);
        var timingType = snapshotType.GetField("Timing")!.FieldType;
        var timings = new List<double>();
        long allocated = 0;
        for (ulong frame = 1; frame <= 240; ++frame)
        {
            var timing = Activator.CreateInstance(timingType, flags, null, [1u, 24u, frame, 1.0/60], null);
            var snapshot = Activator.CreateInstance(snapshotType, flags, null, [input, timing], null);
            var before = GC.GetTotalAllocatedBytes(false);
            var started = Stopwatch.GetTimestamp();
            tick.Invoke(module, [snapshot]);
            timings.Add(Stopwatch.GetElapsedTime(started).TotalMilliseconds);
            allocated += GC.GetTotalAllocatedBytes(false) - before;
        }
        var component = items.GetType().Assembly.GetType("Octaryn.Basegame.Gameplay.Items.WorldItemComponent", true)!;
        var get = ecs.GetType().GetMethod("TryGetComponent")!.MakeGenericMethod(component);
        var sleeping = 0;
        foreach (var entity in entities)
        {
            object?[] values = [entity, null];
            if (!(bool)get.Invoke(ecs, values)!) throw new InvalidOperationException("Distant item count not conserved");
            var state = values[1]!;
            if ((float)component.GetField("Y")!.GetValue(state)! < 0) throw new InvalidOperationException("Item fell through collision");
            if ((bool)component.GetField("Grounded")!.GetValue(state)!) ++sleeping;
        }
        if (sleeping != count) throw new InvalidOperationException($"Items did not settle: {sleeping}/{count}");
        object Stats(IEnumerable<double> source)
        {
            var sorted=source.Order().ToArray();
            return new { median=sorted[sorted.Length/2], p95=sorted[(sorted.Length-1)*95/100],
                p99=sorted[(sorted.Length-1)*99/100], worst=sorted[^1] };
        }
        Console.WriteLine("authority_item_scale=" + JsonSerializer.Serialize(new { status="passed", count, sleeping,
            workload="actual_module_authority_box3d_settling_diagnostics_disabled", replicatedEntityPoses=false,
            settling_ms=Stats(timings.Take(120)), sleeping_ms=Stats(timings.Skip(120)),
            allocated_bytes_per_tick=allocated/240 }));
    }
}
