using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;

internal static class ItemScaleServer
{
    private const BindingFlags Flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;

    public static void Run(string bundle, int count, int awake, double duration, bool visibleToss = false)
    {
        if (count is < 1 or > 10000 || awake < 0 || awake > count || duration < 5 || duration > (visibleToss ? 3600 : 120))
            throw new ArgumentOutOfRangeException(nameof(count));
        bundle = Path.GetFullPath(bundle);
        Console.WriteLine($"authority_item_host process_priority={Process.GetCurrentProcess().PriorityClass} thread_priority={Thread.CurrentThread.Priority} cpu_count={Environment.ProcessorCount}");
        AssemblyLoadContext.Default.Resolving += (_, name) =>
            AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, name.Name + ".dll"));
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Server.dll"));
        var moduleType = assembly.GetType("Octaryn.Server.Modules.ModuleActivator", true)!;
        using var module = (IDisposable)Activator.CreateInstance(moduleType)!;
        var sink = Activator.CreateInstance(assembly.GetType("Octaryn.Server.Host.ConsoleCommandSink", true)!)!;
        if ((int)moduleType.GetMethod("Activate", Flags)!.Invoke(module, [sink])! != 0)
            throw new InvalidOperationException("Authority activation failed");
        var policy = assembly.GetType("Octaryn.Server.Host.NativeHostPolicyLibrary", true)!;
        var startup = policy.GetMethod("CreateStartupFrame", Flags)!.Invoke(null, null)!;
        moduleType.GetMethod("Tick", Flags)!.Invoke(module, [startup]);
        var instance = moduleType.GetField("_instance", Flags)!.GetValue(module)!;
        var ecs = instance.GetType().GetField("_ecs", Flags)!.GetValue(instance)!;
        var items = instance.GetType().GetField("_items", Flags)!.GetValue(instance)!;
        items.GetType().GetField("_diagnostics", Flags)!.SetValue(items, null);
        var spatial = items.GetType().GetField("_spatial", Flags)!.GetValue(items)!;
        var component = items.GetType().Assembly.GetType("Octaryn.Basegame.Gameplay.Items.WorldItemComponent", true)!;
        var get = ecs.GetType().GetMethod("TryGetComponent")!.MakeGenericMethod(component);
        var set = ecs.GetType().GetMethod("SetComponent")!.MakeGenericMethod(component);
        var spawn = items.GetType().GetMethod("Spawn")!;
        var entities = new object[count];
        var columns = count <= 4 ? count : (int)Math.Ceiling(Math.Sqrt(count));
        var spacing = count <= 4 ? .65f : 20f / columns;
        for (var i = 0; i < count; ++i)
            entities[i] = spawn.Invoke(items, [ecs, (ushort)(1 + i % 4), 1u,
                visibleToss ? (i % columns - (columns - 1) / 2f) * spacing : -10f + i % 100 * .2f,
                .6f, visibleToss ? (count <= 4 ? -7f : -7.75f + i / columns * spacing) : i / 100 * .1f,
                0f, 0f, 0f])!;
        // Fixture preparation is excluded from the listener's measured loop.
        for (var i = 0; i < 240; ++i) items.GetType().GetMethod("Tick")!.Invoke(items, [ecs, 1.0 / 60]);
        var settled = 0;
        for (var i = 0; i < count; ++i)
        {
            object?[] values = [entities[i], null];
            if (!(bool)get.Invoke(ecs, values)!) throw new InvalidOperationException("Seeded item disappeared");
            var state = values[1]!;
            if (!(bool)component.GetField("Grounded")!.GetValue(state)! || (float)component.GetField("Y")!.GetValue(state)! < 0)
                throw new InvalidOperationException("Seeded item failed real Box3D floor settling");
            ++settled;
            if (i >= awake) continue;
            component.GetField("Y")!.SetValue(state, visibleToss ? .8f : 100000f);
            component.GetField("VelocityY")!.SetValue(state, visibleToss ? 6f : 0f);
            component.GetField("Grounded")!.SetValue(state, false);
            component.GetField("SleepTimer")!.SetValue(state, 0f);
            set.Invoke(ecs, [entities[i], state]);
            spatial.GetType().GetMethod("Update")!.Invoke(spatial, [entities[i], state]);
            items.GetType().GetMethod("PublishPose", Flags)!.Invoke(items, [entities[i], state, false]);
        }
        var initialPoses = ItemFixtureEvidence.Capture(module, ecs, get, entities);
        ItemFixtureEvidence.Write("initial", initialPoses);
        var control = new ItemFixtureEvidence();
        if (visibleToss) control.WatchInput();
        var world = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_DIR")!;
        var serverType = assembly.GetType("Octaryn.Server.Networking.Remote.RemoteServer", true)!;
        var clock = Stopwatch.StartNew();
        var waves = 1;
        ulong lastFixtureTick = 0;
        var tickProperty = moduleType.GetProperty("AuthorityTickId", Flags)!;
        Func<bool> shutdown = () =>
        {
            if (control.StopRequested || clock.Elapsed.TotalSeconds >= duration) return true;
            if (!visibleToss) return false;
            var tick = (ulong)tickProperty.GetValue(module)!;
            if (tick == lastFixtureTick) return false;
            lastFixtureTick = tick;
            for (var i = 0; i < awake; ++i)
            {
                object?[] values = [entities[i], null];
                if (!(bool)get.Invoke(ecs, values)!) throw new InvalidOperationException("Visible fixture item disappeared");
                var state = values[1]!;
                if ((float)component.GetField("SleepTimer")!.GetValue(state)! <= 0 &&
                    !(bool)component.GetField("Grounded")!.GetValue(state)!) continue;
                ++waves;
                component.GetField("VelocityY")!.SetValue(state, 6f);
                component.GetField("Grounded")!.SetValue(state, false);
                component.GetField("SleepTimer")!.SetValue(state, 0f);
                set.Invoke(ecs, [entities[i], state]);
                spatial.GetType().GetMethod("Update")!.Invoke(spatial, [entities[i], state]);
                items.GetType().GetMethod("PublishPose", Flags)!.Invoke(items, [entities[i], state, false]);
            }
            return false;
        };
        using (var server = (IDisposable)Activator.CreateInstance(serverType, Flags, null,
            [module, Path.Combine(world, "remote-session"), 1u, shutdown], null)!)
        {
            var result = (int)serverType.GetMethod("Execute")!.Invoke(server, ["127.0.0.1", 0])!;
            if (result != 0) throw new InvalidOperationException($"Listener failed: {result}");
        }
        var actualAwake = 0;
        foreach (var entity in entities)
        {
            object?[] values = [entity, null];
            if (!(bool)get.Invoke(ecs, values)!) throw new InvalidOperationException("Count conservation failed");
            if (!(bool)component.GetField("Grounded")!.GetValue(values[1])!) ++actualAwake;
        }
        if (actualAwake != awake) throw new InvalidOperationException($"Awake workload changed {awake}->{actualAwake}");
        var finalPoses = ItemFixtureEvidence.Capture(module, ecs, get, entities);
        ItemFixtureEvidence.Write("final", finalPoses);
        var stablePoses = initialPoses.SequenceEqual(finalPoses);
        if (awake == 0 && !stablePoses) throw new InvalidOperationException("Sleeping fixture poses changed");
        if (!initialPoses.Select(p => (p.EntityId, p.Generation, p.ItemId, p.Count)).SequenceEqual(
            finalPoses.Select(p => (p.EntityId, p.Generation, p.ItemId, p.Count))))
            throw new InvalidOperationException("Fixture identity or quantity conservation failed");
        Console.WriteLine("authority_item_listener=" + JsonSerializer.Serialize(new
        {
            status = "passed", count, awake, settledBeforeMeasurement = settled, durationSeconds = clock.Elapsed.TotalSeconds,
            workload = visibleToss ? (awake == 0 ? "stable_authoritative_sleeping_items" :
                "visible_contact_relaunch_fixture_includes_reflection_injection_cost") :
                "actual_listener_module_box3d_sustained_airborne_and_sleeping", waves,
            finalAwake = actualAwake, contactStressQualified = false, stablePoses,
            stoppedByOwner = control.StopRequested, quantityConserved = true
        }));
        module.Dispose();
        assembly.GetType("Octaryn.Server.LiveDebugLog", true)!.GetMethod("Shutdown", Flags)!.Invoke(null, null);
    }
}
