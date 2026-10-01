using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;

internal static class TraversalProbe
{
    public static void Run(string bundle, string endpoint, string runtime, double seconds)
    {
        if (seconds < 10 || seconds > 7200) throw new ArgumentOutOfRangeException(nameof(seconds));
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
        if (!(bool)Call("Start", endpoint, runtime, 8000)!) throw new InvalidOperationException("Connection failed");
        void Intent(byte kind, object value)
        {
            if (!(bool)Call("SubmitIntent", kind, JsonSerializer.SerializeToUtf8Bytes(value))!)
                throw new InvalidOperationException("Intent channel rejected bounded traversal request");
        }
        Intent(1, new { version = 1, epoch = 1, centerChunkX = 0, centerChunkZ = 0, radius = 4 });
        using var trace = new StreamWriter(Path.Combine(runtime, "traversal.csv"));
        trace.WriteLine("seconds,source_tick,ack,generated,pending,x,y,z,distance,turns,boundaries,ack_stall_seconds");
        var pending = new List<(ulong Sequence, object Command)>(64);
        var clock = Stopwatch.StartNew();
        double? started = null;
        double nextCommand = 0, nextTrace = 0, lastAck = 0, maximumAckStall = 0, distance = 0;
        float x = 0, y = 0, z = 0, direction = 1, minimumY = float.MaxValue, maximumY = float.MinValue;
        float minimumX = float.MaxValue, maximumX = float.MinValue;
        ulong generated = 0, acknowledged = 0, sourceTick = 0;
        int poses = 0, turns = 0, boundaries = 0, previousTile = 0, maximumPending = 0, drops = 0;
        bool dropped = false, actionAcknowledged = false;
        var poseArgs = new object?[] { null };
        var eventArgs = new object?[] { null };
        var ackArgs = new object?[] { 0UL, 0UL };
        while (!started.HasValue || clock.Elapsed.TotalSeconds - started.Value < seconds)
        {
            var now = clock.Elapsed.TotalSeconds;
            if (!started.HasValue && now > 60) throw new TimeoutException("No authoritative pose before readiness deadline");
            if (!(bool)type.GetProperty("IsRunning", members)!.GetValue(transport)!)
                throw new InvalidOperationException("Traversal transport stopped: " + type.GetProperty("Status", members)!.GetValue(transport));
            if ((bool)Call("TryPollPose", poseArgs)!)
            {
                var pose = poseArgs[0]!;
                var poseType = pose.GetType();
                var tick = (ulong)poseType.GetField("SourceTick")!.GetValue(pose)!;
                var ack = (ulong)poseType.GetField("AcknowledgedInputFrame")!.GetValue(pose)!;
                var px = (float)poseType.GetField("X")!.GetValue(pose)!;
                var py = (float)poseType.GetField("Y")!.GetValue(pose)!;
                var pz = (float)poseType.GetField("Z")!.GetValue(pose)!;
                if (tick < sourceTick || ack < acknowledged || ack > generated ||
                    !float.IsFinite(px) || !float.IsFinite(py) || !float.IsFinite(pz) ||
                    py < 1.2f || py > 5 || MathF.Abs(pz + 3) > 1 || px < -2 || px > 770)
                    throw new InvalidOperationException($"Invalid authoritative route pose/ACK: {px},{py},{pz} tick={tick} ack={ack}");
                if (poses > 0) distance += Math.Abs(px - x);
                else { started = now; nextCommand = now; lastAck = now; }
                if (ack > acknowledged) { maximumAckStall = Math.Max(maximumAckStall, now-lastAck); lastAck = now; }
                (x, y, z, sourceTick, acknowledged) = (px, py, pz, tick, ack);
                pending.RemoveAll(command => command.Sequence <= ack);
                var tile = (int)MathF.Floor((x + 12) / 24);
                if (poses > 0 && tile != previousTile) ++boundaries;
                previousTile = tile;
                minimumY = Math.Min(minimumY, y); maximumY = Math.Max(maximumY, y);
                minimumX = Math.Min(minimumX, x); maximumX = Math.Max(maximumX, x);
                ++poses;
                if (direction > 0 && x >= 760) { direction = -1; ++turns; }
                else if (direction < 0 && x <= 8) { direction = 1; ++turns; }
            }
            if (started.HasValue)
            {
                if (now-lastAck > 5) throw new TimeoutException("Authoritative ACK held for five seconds; inspect collision readiness diagnostics");
                for (var step = 0; step < 8 && nextCommand <= now && pending.Count < 64; ++step)
                {
                    ++generated;
                    pending.Add((generated, Activator.CreateInstance(commandType,
                        [generated, 2u, 1u, direction, 0f, 0f, 0f, 0f, 1])!));
                    nextCommand += 1.0 / 60;
                }
                if (pending.Count == 64) nextCommand = Math.Max(nextCommand, now);
                maximumPending = Math.Max(maximumPending, pending.Count);
                if (pending.Count != 0)
                {
                    var batch = Array.CreateInstance(commandType, pending.Count);
                    for (int i = 0; i < pending.Count; ++i) batch.SetValue(pending[i].Command, i);
                    if (!(bool)Call("SubmitCommands", batch)!) throw new InvalidOperationException("Command journal rejected");
                }
                if (!dropped && now-started.Value >= 2)
                {
                    Intent(5, new { version = 1, epoch = 902, seq = 1, actions = new[] { "inventory.drop" } });
                    dropped = true;
                }
                if (now >= nextTrace)
                {
                    trace.WriteLine(FormattableString.Invariant($"{now-started.Value:F3},{sourceTick},{acknowledged},{generated},{pending.Count},{x:F6},{y:F6},{z:F6},{distance:F3},{turns},{boundaries},{now-lastAck:F3}"));
                    trace.Flush();
                    nextTrace = now + 1;
                }
            }
            while ((bool)Call("TryPollModuleEvent", eventArgs)!)
                if ((ulong)eventArgs[0]!.GetType().GetField("Payload0")!.GetValue(eventArgs[0])! == 2) ++drops;
            if ((bool)Call("TryPollActionAcknowledgement", ackArgs)! && (ulong)ackArgs[0]! == 902 && (ulong)ackArgs[1]! == 1)
                actionAcknowledged = true;
            Thread.Sleep(8);
        }
        if (distance < seconds*5 || boundaries < seconds/6 || poses < seconds*10 || acknowledged < (ulong)(seconds*40) ||
            drops != 1 || !actionAcknowledged || maximumPending > 64)
            throw new InvalidOperationException($"Traversal failed: distance={distance} boundaries={boundaries} poses={poses} ack={acknowledged} drops={drops} actionAck={actionAcknowledged}");
        if (Directory.EnumerateFiles(runtime, "*.json").Any()) throw new InvalidOperationException("Production JSON mailboxes appeared");
        var result = new { status = "passed", seconds, distance, turns, boundaries, poses, acknowledged, generated,
            maximumPending, maximumAckStall, minimumY, maximumY, minimumX, maximumX, drops, actionAcknowledged,
            scope = "real authority and managed client transport; no graphical client or prediction qualification" };
        File.WriteAllText(Path.Combine(runtime, "result.json"), JsonSerializer.Serialize(result, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine(JsonSerializer.Serialize(result));
    }
}
