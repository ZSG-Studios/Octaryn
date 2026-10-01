using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;

internal static class EventJournalProbe
{
    public static void Run(string serverPath, string clientPath)
    {
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
        serverPath = Path.GetFullPath(serverPath);
        clientPath = Path.GetFullPath(clientPath);
        AssemblyLoadContext.Default.Resolving += (_, name) => AssemblyLoadContext.Default.LoadFromAssemblyPath(
            Path.Combine(serverPath, name.Name + ".dll"));
        var serverAssembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(serverPath, "Octaryn.Server.dll"));
        var clientAssembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(clientPath, "Octaryn.Client.dll"));
        var shared = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(serverPath, "Octaryn.Shared.dll"));
        var envelopeType = shared.GetType("Octaryn.Shared.Networking.Remote.SessionEventEnvelope", true)!;
        var dataType = shared.GetType("Octaryn.Shared.Networking.Remote.ModuleEventData", true)!;
        var serverType = serverAssembly.GetType("Octaryn.Server.Networking.Remote.RemoteSession", true)!;
        var server = RuntimeHelpers.GetUninitializedObject(serverType);
        var queue = Activator.CreateInstance(typeof(Queue<>).MakeGenericType(envelopeType))!;
        serverType.GetField("_eventJournal", flags)!.SetValue(server, queue);
        serverType.GetField("_sessionHigh", flags)!.SetValue(server, 1UL);
        object? Server(string name, params object[] args) => serverType.GetMethod(name, flags)!.Invoke(server, args);
        int Count() => (int)queue.GetType().GetProperty("Count")!.GetValue(queue)!;
        object Data(ulong id)
        {
            var data = Activator.CreateInstance(dataType)!;
            dataType.GetField("EventId")!.SetValue(data, id);
            dataType.GetField("Payload0")!.SetValue(data, 2UL);
            return data;
        }
        for (ulong i = 1; i <= 256; ++i) Server("Broadcast", Data(i));
        if ((bool)Server("Broadcast", Data(257))!) throw new Exception("Overflow silently admitted");
        if (Count() != 256) throw new Exception("Overflow lost retained events");
        var hostType = serverAssembly.GetType("Octaryn.Server.Host.ServerHostApiProvider", true)!;
        var host = RuntimeHelpers.GetUninitializedObject(hostType);
        var actions = new Queue<string>();
        actions.Enqueue("inventory.drop");
        hostType.GetField("_uiActions", flags)!.SetValue(host, actions);
        hostType.GetField("_replicationChannel", flags)!.SetValue(host, server);
        var ui = hostType.GetMethod("GetUiApi", flags)!.Invoke(host, null)!;
        var poll = ui.GetType().GetMethod("TryPollAction", flags)!;
        var actionOutput = new object?[] { null };
        if ((bool)poll.Invoke(ui, actionOutput)! || actions.Count != 1)
            throw new Exception("Full receipt journal dequeued an admitted action");
        if ((bool)hostType.GetMethod("EnqueueUiAction", flags)!.Invoke(host, ["inventory.drop"])! ||
            (bool)hostType.GetMethod("AcknowledgeUiActions", flags)!.Invoke(host, [1UL, 1UL])!)
            throw new Exception("Full receipt journal admitted or acknowledged new actions");
        Server("AcknowledgeEvents", BitConverter.GetBytes(999UL));
        if (Count() != 256) throw new Exception("Future acknowledgement erased events");
        Server("AcknowledgeEvents", BitConverter.GetBytes(128UL));
        if (Count() != 128) throw new Exception("Partial acknowledgement count");
        if (!(bool)poll.Invoke(ui, actionOutput)! || (string)actionOutput[0]! != "inventory.drop" || actions.Count != 0)
            throw new Exception("Retained action did not resume after receipt capacity returned");
        Server("AcknowledgeEvents", BitConverter.GetBytes(64UL));
        if (Count() != 128) throw new Exception("Stale acknowledgement count");

        var clientType = clientAssembly.GetType("Octaryn.Client.Host.Remote.RemoteTransportClient", true)!;
        using var client = (IDisposable)Activator.CreateInstance(clientType, true)!;
        object Envelope(ulong sequence)
        {
            var item = Activator.CreateInstance(envelopeType)!;
            envelopeType.GetField("Sequence")!.SetValue(item, sequence);
            envelopeType.GetField("Data")!.SetValue(item, Data(sequence));
            return item;
        }
        var receive = clientType.GetMethod("OnModuleEvent", flags)!;
        for (ulong i = 1; i <= 256; ++i) receive.Invoke(client, [Envelope(i)]);
        var slots = (byte[]?[])clientType.GetField("_intentSlots", flags)!.GetValue(client)!;
        if (slots[6] is not null) throw new Exception("Unconsumed receipts were acknowledged");
        for (ulong i = 1; i <= 256; ++i) receive.Invoke(client, [Envelope(i)]);
        var output = new object?[] { null };
        for (ulong i = 1; i <= 256; ++i)
        {
            if (!(bool)clientType.GetMethod("TryPollModuleEvent", flags)!.Invoke(client, output)! ||
                (ulong)dataType.GetField("EventId")!.GetValue(output[0])! != i)
                throw new Exception("Replay duplicated or reordered a receipt");
        }
        if ((bool)clientType.GetMethod("TryPollModuleEvent", flags)!.Invoke(client, output)!)
            throw new Exception("Replay retained duplicate receipts");
        if (slots[6] is not { } ack || BitConverter.ToUInt64(ack) != 256)
            throw new Exception("Consumed receipt acknowledgement did not advance");
        receive.Invoke(client, [Envelope(258)]);
        if (!((string)clientType.GetProperty("Status", flags)!.GetValue(client)!).Contains("sequence gap"))
            throw new Exception("Sequence gap did not fail visibly");
        Console.WriteLine("event_journal_probe=passed retained=256 partial_ack=128 duplicate_replay=256 gap_rejected=1 action_backpressure=1 consumer_ack=1");
    }
}
