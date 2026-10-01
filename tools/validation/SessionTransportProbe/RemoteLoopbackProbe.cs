using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;

internal static class RemoteLoopbackProbe
{
    public static void Run(string bundle, string endpoint, string runtime, bool reconnect = false)
    {
        bundle = Path.GetFullPath(bundle);
        Directory.CreateDirectory(runtime);
        AssemblyLoadContext.Default.Resolving += (_, name) =>
            AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, name.Name + ".dll"));
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Client.dll"));
        var shared = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Shared.dll"));
        var type = assembly.GetType("Octaryn.Client.Host.Remote.RemoteTransportClient", true)!;
        var commandType = shared.GetType("Octaryn.Shared.Networking.Remote.PlayerCommand", true)!;
        const BindingFlags members = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;
        using var transport = (IDisposable)Activator.CreateInstance(type, true)!;
        object? Call(string name, params object?[] values) => type.GetMethod(name, members)!.Invoke(transport, values);
        if (!(bool)Call("Start", endpoint, runtime, 8000)!) throw new InvalidOperationException("Remote connection failed");
        void Intent(byte kind, object value)
        {
            if (!(bool)Call("SubmitIntent", kind, JsonSerializer.SerializeToUtf8Bytes(value))!)
                throw new InvalidOperationException("Intent rejected");
        }
        Intent(1, new { version = 1, epoch = 1, centerChunkX = 0, centerChunkZ = 0, radius = 4 });
        ulong frame = 0, acknowledged = 0, tick = 0;
        var poses = 0;
        var dropCount = 0;
        var ackedAction = false;
        var clock = Stopwatch.StartNew();
        var poseArgs = new object?[] { null };
        var eventArgs = new object?[] { null };
        var ackArgs = new object?[] { 0UL, 0UL };
        var dropped = false;
        var disconnected = false;
        var resumed = false;
        ulong beforeDisconnect = 0;
        var pending = new List<(ulong Sequence, object Command)>(64);
        var itemRevisions = new HashSet<ulong>();
        var observedItems = new HashSet<(ulong Id, ulong Generation)>();
        var itemTicks = new HashSet<ulong>();
        var itemSnapshotAfterResume = false;
        while (clock.Elapsed.TotalSeconds < (reconnect ? 15 : 8))
        {
            if ((bool)Call("TryPollPose", poseArgs)!)
            {
                var pose = poseArgs[0]!;
                var poseType = pose.GetType();
                var received = (ulong)poseType.GetField("SourceTick")!.GetValue(pose)!;
                if (received < tick) throw new InvalidOperationException("Pose tick regressed");
                tick = received;
                var ack = (ulong)poseType.GetField("AcknowledgedInputFrame")!.GetValue(pose)!;
                if (ack < acknowledged) throw new InvalidOperationException($"Authority ACK regressed {acknowledged}->{ack} afterDisconnect={disconnected}");
                acknowledged = ack;
                pending.RemoveAll(command => command.Sequence <= ack);
                if (disconnected && ack > beforeDisconnect + 30) resumed = true;
                if (!float.IsFinite((float)poseType.GetField("Y")!.GetValue(pose)!))
                    throw new InvalidOperationException("Nonfinite authority pose");
                ++poses;
            }
            if (poses > 0)
            {
                if (pending.Count < 64)
                {
                    ++frame;
                    pending.Add((frame, Activator.CreateInstance(commandType,
                        [frame, 0u, 1u, 0f, 0f, 0f, -0.25f, 0.6f, 1])!));
                }
                var count = pending.Count;
                if (count > 0)
                {
                    var batch = Array.CreateInstance(commandType, count);
                    for (var i = 0; i < count; ++i)
                        batch.SetValue(pending[i].Command, i);
                    if (!(bool)Call("SubmitCommands", batch)!) throw new InvalidOperationException("Commands rejected");
                }
                if (!dropped && frame >= 60)
                {
                    Intent(5, new { version = 1, epoch = 901, seq = 1, actions = new[] { "inventory.drop" } });
                    dropped = true;
                }
            }
            while ((bool)Call("TryPollModuleEvent", eventArgs)!)
            {
                var data = eventArgs[0]!;
                if ((ulong)data.GetType().GetField("Payload0")!.GetValue(data)! == 2) ++dropCount;
            }
            if ((bool)Call("TryPollActionAcknowledgement", ackArgs)! && (ulong)ackArgs[0]! == 901 && (ulong)ackArgs[1]! == 1)
                ackedAction = true;
            var itemSnapshot = type.GetProperty("WorldItems", members)!.GetValue(transport)!;
            var itemRevision = (ulong)itemSnapshot.GetType().GetProperty("Revision")!.GetValue(itemSnapshot)!;
            var itemPoses = (Array)itemSnapshot.GetType().GetProperty("Poses")!.GetValue(itemSnapshot)!;
            if (resumed && itemPoses.Length > 0) itemSnapshotAfterResume = true;
            if (itemRevisions.Add(itemRevision))
            {
                foreach (var visual in itemPoses)
                {
                    var item = visual!.GetType().GetField("Current")!.GetValue(visual)!;
                    var itemType = item.GetType();
                    var id = (ulong)itemType.GetField("EntityId")!.GetValue(item)!;
                    var generation = (ulong)itemType.GetField("Generation")!.GetValue(item)!;
                    observedItems.Add((id, generation));
                    itemTicks.Add((ulong)itemType.GetField("SourceTick")!.GetValue(item)!);
                    if (!float.IsFinite((float)itemType.GetField("Y")!.GetValue(item)!) || generation == 0)
                        throw new InvalidOperationException("Invalid typed item pose");
                }
            }
            if (reconnect && !disconnected && acknowledged >= 150 && ackedAction)
            {
                var peer = type.GetField("_peer", members)!.GetValue(transport)!;
                peer.GetType().GetMethod("Disconnect", Type.EmptyTypes)!.Invoke(peer, null);
                beforeDisconnect = acknowledged;
                disconnected = true;
                Console.WriteLine($"reconnect_disconnect ack={beforeDisconnect}");
            }
            Thread.Sleep(16);
        }
        if (poses < 30 || acknowledged < 100 || dropCount != 1 || !ackedAction || (reconnect && !resumed))
            throw new InvalidOperationException($"Typed session failed poses={poses} ack={acknowledged} drops={dropCount} actionAck={ackedAction}");
        if (Directory.EnumerateFiles(runtime, "*.json").Any()) throw new InvalidOperationException("Production transport wrote JSON mailbox files");
        if (observedItems.Count == 0 || itemTicks.Count < 2 || (reconnect && !itemSnapshotAfterResume))
            throw new InvalidOperationException($"Item replication missing: items={observedItems.Count} ticks={itemTicks.Count} resumed={itemSnapshotAfterResume}");
        Console.WriteLine(JsonSerializer.Serialize(new { status = "passed", poses, acknowledged, dropCount, ackedAction,
            disconnected, resumed, mailboxFiles = 0, worldItems = observedItems.Count, worldItemTicks = itemTicks.Count,
            itemSnapshotAfterResume }));
    }
}
