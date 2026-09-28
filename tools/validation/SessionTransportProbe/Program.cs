using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using System.Text.Json;

if (args[0] == "--item-scale-server")
{
    ItemScaleServer.Run(args[1], int.Parse(args[2]), int.Parse(args[3]), double.Parse(args[4]),
        args.Length > 5 && args[5] == "visible-toss");
    return;
}

if (args[0] == "--item-scale-client")
{
    ItemScaleClient.Run(args[1], args[2], args[3], int.Parse(args[4]), double.Parse(args[5]));
    return;
}

if (args[0] == "--player-spawn")
{
    PlayerSpawnProbe.Run(args[1], args[2], args[3]);
    return;
}

if (args[0] == "--backpressure")
{
    BackpressureProbe.Run(args[1], args[2], args[3]);
    return;
}

if (args[0] == "--event-journal")
{
    EventJournalProbe.Run(args[1], args[2]);
    return;
}

if (args[0] == "--authority-items")
{
    AuthorityItemProbe.Run(args[1], int.Parse(args[2]));
    return;
}

if (args[0] is "--live" or "--reconnect")
{
    RemoteLoopbackProbe.Run(args[1], args[2], args[3], args[0] == "--reconnect");
    return;
}

if (args[0] == "--traversal")
{
    TraversalProbe.Run(args[1], args[2], args[3], double.Parse(args[4], System.Globalization.CultureInfo.InvariantCulture));
    return;
}

// Exercise the built authority ingress and real bounded host queue without a
// renderer or substitute implementation. Reflection only bypasses world startup.
var bundle = Path.GetFullPath(args[0]);
var evidence = Path.GetFullPath(args[1]);
Directory.CreateDirectory(evidence);
AssemblyLoadContext.Default.Resolving += (_, name) =>
    AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, name.Name + ".dll"));
var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Server.dll"));
const BindingFlags members = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
var hostType = assembly.GetType("Octaryn.Server.Host.ServerHostApiProvider", true)!;
var moduleType = assembly.GetType("Octaryn.Server.Modules.ModuleActivator", true)!;
var ingressType = assembly.GetType("Octaryn.Server.ChunkStreamProcessBridge", true)!;
var host = RuntimeHelpers.GetUninitializedObject(hostType);
var queue = new Queue<string>();
hostType.GetField("_uiActions", members)!.SetValue(host, queue);
var module = RuntimeHelpers.GetUninitializedObject(moduleType);
moduleType.GetField("_managedApis", members)!.SetValue(module, host);
var ingress = ingressType.GetMethod("ReadUiActionIntent", members)!;
var mailbox = Path.Combine(evidence, "ui_action.json");
var checks = new List<string>();

void Check(bool condition, string name)
{
    if (!condition) throw new InvalidOperationException(name);
    checks.Add(name);
}
void Read() => ingress.Invoke(null, [module, mailbox]);
void Write(ulong epoch, ulong sequence, params string[] actions)
{
    File.WriteAllText(mailbox, JsonSerializer.Serialize(new { version = 1, epoch, seq = sequence, actions }));
    Read();
}
bool Ack(ulong epoch, ulong sequence)
{
    using var json = JsonDocument.Parse(File.ReadAllText(mailbox + ".ack"));
    return json.RootElement.GetProperty("epoch").GetUInt64() == epoch &&
        json.RootElement.GetProperty("seq").GetUInt64() == sequence;
}

Write(41, 3, "select.1", "select.0", "drop.1");
Check(queue.SequenceEqual(["select.1", "select.0", "drop.1"]) && Ack(41, 3), "ordered initial batch");
Read();
Check(queue.Count == 3, "identical journal does not replay");
Write(41, 5, "select.1", "select.0", "drop.1", "drop.2", "interact");
Check(queue.SequenceEqual(["select.1", "select.0", "drop.1", "drop.2", "interact"]) && Ack(41, 5), "overlapping journal accepts only suffix");
foreach (var malformed in new[] { "{", "null", "[]", "1", "{\"seq\":\"bad\",\"actions\":[]}",
    "{\"version\":1,\"epoch\":99,\"seq\":1,\"actions\":[null]}" })
{
    File.WriteAllText(mailbox, malformed);
    Read();
    Check(queue.Count == 5 && Ack(41, 5), "malformed rejected: " + malformed);
}
Write(41, 7, "gap");
Check(queue.Count == 5 && Ack(41, 5), "sequence gap rejected");
Write(41, 262, Enumerable.Repeat("overflow", 257).ToArray());
Check(queue.Count == 5 && Ack(41, 5), "oversized batch rejected atomically");
queue.Clear();
var enqueue = hostType.GetMethod("EnqueueUiAction", members)!;
for (var i = 0; i < 255; i++) Check((bool)enqueue.Invoke(host, ["queued." + i])!, "preload " + i);
Write(41, 7, "sixth", "seventh");
Check(queue.Count == 256 && queue.Last() == "sixth" && Ack(41, 6), "full queue acknowledges only admitted prefix");
Read();
Check(queue.Count == 256 && Ack(41, 6), "full queue replay remains bounded");
Check(!(bool)enqueue.Invoke(host, ["overflow"])! && queue.Count == 256, "host refuses overflow explicitly");
queue.Clear();
Read();
Check(queue.SequenceEqual(["seventh"]) && Ack(41, 7), "retained suffix resumes after backpressure");
Read();
Check(queue.Count == 1, "resumed suffix does not replay");
Write(42, 1, "new.session");
Check(queue.SequenceEqual(["new.session"]) && Ack(42, 1), "new epoch resets sequence and stale queue");

var eventType = assembly.GetType("Octaryn.Server.Host.LocalModuleEventMailbox", true)!;
var eventPath = Path.Combine(evidence, "module_events.json");
var events = Activator.CreateInstance(eventType, members, null, [eventPath], null)!;
var broadcast = eventType.GetMethod("Broadcast", members)!;
var dataType = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Shared.dll"))
    .GetType("Octaryn.Shared.Networking.Remote.ModuleEventData", true)!;
object Data(ulong id)
{
    var data = Activator.CreateInstance(dataType)!;
    dataType.GetField("EventId")!.SetValue(data, id);
    dataType.GetField("Payload0")!.SetValue(data, ulong.MaxValue - 1);
    dataType.GetField("Payload1")!.SetValue(data, ulong.MaxValue - 2);
    dataType.GetField("Payload2")!.SetValue(data, ulong.MaxValue - 3);
    return data;
}
for (ulong i = 1; i <= 256; i++) broadcast.Invoke(events, [Data(i)]);
Check(new FileInfo(eventPath).Length > 16384 && new FileInfo(eventPath).Length < 65536, "worst case event journal fits explicit read bound");
Check(!(bool)broadcast.Invoke(events, [Data(257)])!, "event backpressure refuses overflow without eviction");
File.WriteAllText(eventPath + ".ack", "100");
broadcast.Invoke(events, [Data(257)]);
using (var json = JsonDocument.Parse(File.ReadAllText(eventPath)))
{
    var entries = json.RootElement.GetProperty("events");
    Check(entries.GetArrayLength() == 157 && entries[0].GetProperty("seq").GetUInt64() == 101 &&
        entries[156].GetProperty("seq").GetUInt64() == 257, "event ACK prunes only consumed prefix and preserves ordering");
}
ClientStatusProbe.Run(bundle);
CommandDependencyProbe.Run(assembly, evidence);
checks.Add("client status reads do not wait behind transport mutex");
File.WriteAllText(Path.Combine(evidence, "result.json"), JsonSerializer.Serialize(new { status = "passed", checks }, new JsonSerializerOptions { WriteIndented = true }));
Console.WriteLine($"session_transport=passed checks={checks.Count} evidence={evidence}");
