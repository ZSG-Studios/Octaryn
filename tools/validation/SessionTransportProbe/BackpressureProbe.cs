using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;

internal static class BackpressureProbe
{
    public static void Run(string bundle, string endpoint, string runtime)
    {
        bundle = Path.GetFullPath(bundle);
        Directory.CreateDirectory(runtime);
        AssemblyLoadContext.Default.Resolving += (_, name) => AssemblyLoadContext.Default.LoadFromAssemblyPath(
            Path.Combine(bundle, name.Name + ".dll"));
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Client.dll"));
        var type = assembly.GetType("Octaryn.Client.Host.Remote.RemoteTransportClient", true)!;
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
        using var transport = (IDisposable)Activator.CreateInstance(type, true)!;
        object? Call(string name, params object?[] values) => type.GetMethod(name, flags)!.Invoke(transport, values);
        if (!(bool)Call("Start", endpoint, runtime, 8000)!) throw new Exception("Connection failed");
        void Intent(byte kind, object value)
        {
            if (!(bool)Call("SubmitIntent", kind, JsonSerializer.SerializeToUtf8Bytes(value))!)
                throw new Exception("Intent rejected");
        }
        Intent(1, new { version = 1, epoch = 1, centerChunkX = 0, centerChunkZ = 0, radius = 4 });
        var timer = Stopwatch.StartNew();
        var ack = new object?[] { 0UL, 0UL };
        var pose = new object?[] { null };
        var receipt = new object?[] { null };
        ulong sent = 0, acknowledged = 0;
        double? fullAt = null;
        int posesWhileFull = 0, drops = 0;
        bool resumed = false;
        while (timer.Elapsed.TotalSeconds < 25)
        {
            if (!(bool)type.GetProperty("IsRunning", flags)!.GetValue(transport)!)
                throw new Exception("Transport stopped under backpressure");
            if ((bool)Call("TryPollPose", pose)! && fullAt.HasValue && !resumed) ++posesWhileFull;
            if ((bool)Call("TryPollActionAcknowledgement", ack)! && (ulong)ack[0]! == 903)
                acknowledged = (ulong)ack[1]!;
            if (sent == acknowledged && sent < 256)
            {
                ++sent;
                Intent(5, new { version = 1, epoch = 903, seq = sent, actions = new[] { "inventory.select.0" } });
            }
            if (acknowledged == 256 && !fullAt.HasValue)
            {
                fullAt = timer.Elapsed.TotalSeconds;
                sent = 257;
                Intent(5, new { version = 1, epoch = 903, seq = sent, actions = new[] { "inventory.drop" } });
            }
            if (fullAt.HasValue && timer.Elapsed.TotalSeconds - fullAt < 1)
            {
                if (acknowledged > 256) throw new Exception("Authority acknowledged action beyond full receipt journal");
            }
            else if (fullAt.HasValue)
            {
                resumed = true;
                while ((bool)Call("TryPollModuleEvent", receipt)!)
                    if ((ulong)receipt[0]!.GetType().GetField("Payload0")!.GetValue(receipt[0])! == 2) ++drops;
                if (acknowledged == 257 && drops == 1) break;
            }
            Thread.Sleep(2);
        }
        if (!resumed || acknowledged != 257 || drops != 1 || posesWhileFull < 5)
            throw new Exception($"Backpressure did not resume: ack={acknowledged} drops={drops} heldPoses={posesWhileFull}");
        // Continue consuming to expose duplicate execution after the retained intent retires.
        var until = timer.Elapsed.TotalSeconds + .5;
        while (timer.Elapsed.TotalSeconds < until)
        {
            while ((bool)Call("TryPollModuleEvent", receipt)!)
                if ((ulong)receipt[0]!.GetType().GetField("Payload0")!.GetValue(receipt[0])! == 2) ++drops;
            Thread.Sleep(5);
        }
        if (drops != 1) throw new Exception("Retried action dropped multiple items");
        Console.WriteLine(JsonSerializer.Serialize(new { status = "passed", journalBound = 256,
            heldSeconds = 1, posesWhileFull, acknowledged, drops, resumed }));
    }
}
