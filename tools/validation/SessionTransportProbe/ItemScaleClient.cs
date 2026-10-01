using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;

internal static class ItemScaleClient
{
    public static void Run(string bundle, string endpoint, string runtime, int expected, double duration)
    {
        bundle = Path.GetFullPath(bundle);
        Directory.CreateDirectory(runtime);
        AssemblyLoadContext.Default.Resolving += (_, name) =>
            AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, name.Name + ".dll"));
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Client.dll"));
        const BindingFlags flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;
        var type = assembly.GetType("Octaryn.Client.Host.Remote.RemoteTransportClient", true)!;
        using var transport = (IDisposable)Activator.CreateInstance(type, true)!;
        object? Call(string name, params object?[] values) => type.GetMethod(name, flags)!.Invoke(transport, values);
        if (!(bool)Call("Start", endpoint, runtime, 8000)!) throw new InvalidOperationException("Connect failed");
        if (!(bool)Call("SubmitIntent", (byte)1, JsonSerializer.SerializeToUtf8Bytes(new
            { version = 1, epoch = 1, centerChunkX = 0, centerChunkZ = 0, radius = 4 }))!)
            throw new InvalidOperationException("Initial interest rejected");
        var clock = Stopwatch.StartNew();
        var revisions = new HashSet<ulong>();
        var fullSnapshots = 0;
        var poses = 0;
        var poseArgs = new object?[] { null };
        var maximumItems = 0;
        double firstComplete = -1;
        double lastComplete = -1, maximumSnapshotInterval = 0;
        ulong maximumAwakeTickSpread = 0;
        while (clock.Elapsed.TotalSeconds < duration)
        {
            while ((bool)Call("TryPollPose", poseArgs)!) ++poses;
            var snapshot = type.GetProperty("WorldItems", flags)!.GetValue(transport)!;
            var revision = (ulong)snapshot.GetType().GetProperty("Revision")!.GetValue(snapshot)!;
            if (revisions.Add(revision))
            {
                var items = (Array)snapshot.GetType().GetProperty("Poses")!.GetValue(snapshot)!;
                maximumItems = Math.Max(maximumItems, items.Length);
                if (items.Length > 0 && items.Length != expected)
                    throw new InvalidOperationException($"Partial baseline published {items.Length}/{expected}");
                if (items.Length == expected)
                {
                    if (firstComplete < 0) firstComplete = clock.Elapsed.TotalMilliseconds;
                    if (lastComplete >= 0) maximumSnapshotInterval = Math.Max(maximumSnapshotInterval,
                        clock.Elapsed.TotalMilliseconds - lastComplete);
                    lastComplete = clock.Elapsed.TotalMilliseconds;
                    ++fullSnapshots;
                    var ids = new HashSet<ulong>();
                    ulong oldestAwake = ulong.MaxValue, newestAwake = 0;
                    foreach (var visual in items)
                    {
                        var pose = visual!.GetType().GetField("Current")!.GetValue(visual)!;
                        var poseType = pose.GetType();
                        if (!ids.Add((ulong)poseType.GetField("EntityId")!.GetValue(pose)!) ||
                            (ulong)poseType.GetField("Generation")!.GetValue(pose)! == 0 ||
                            !float.IsFinite((float)poseType.GetField("Y")!.GetValue(pose)!))
                            throw new InvalidOperationException("Invalid replicated item identity/pose");
                        if (((uint)poseType.GetField("Flags")!.GetValue(pose)! & 2) == 0)
                        {
                            var tick = (ulong)poseType.GetField("SourceTick")!.GetValue(pose)!;
                            oldestAwake = Math.Min(oldestAwake, tick);
                            newestAwake = Math.Max(newestAwake, tick);
                        }
                    }
                    if (oldestAwake != ulong.MaxValue)
                        maximumAwakeTickSpread = Math.Max(maximumAwakeTickSpread, newestAwake - oldestAwake);
                }
            }
            Thread.Sleep(16);
        }
        if (maximumItems != expected || fullSnapshots < 2 || poses < 30)
            throw new InvalidOperationException($"Stream incomplete items={maximumItems} snapshots={fullSnapshots} poses={poses}");
        Console.WriteLine("item_scale_client=" + JsonSerializer.Serialize(new
            { status = "passed", expected, maximumItems, fullSnapshots, poses, firstCompleteMilliseconds = firstComplete,
                maximumSnapshotIntervalMilliseconds = maximumSnapshotInterval, maximumAwakeTickSpread,
                measurementScope = "reflection validation client; not native frame-thread timing" }));
    }
}
