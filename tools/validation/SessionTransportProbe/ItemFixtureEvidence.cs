using System.Collections;
using System.Reflection;
using System.Text.Json;

internal sealed class ItemFixtureEvidence
{
    private const BindingFlags Flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;
    private int _stop;
    public bool StopRequested => Volatile.Read(ref _stop) != 0;

    public void WatchInput()
    {
        if (!Console.IsInputRedirected) return;
        _ = Task.Run(async () =>
        {
            while (await Console.In.ReadLineAsync() is { } line)
            {
                if (line != "stop") continue;
                Interlocked.Exchange(ref _stop, 1);
                return;
            }
        });
    }

    internal sealed record Pose(ulong EntityId, ulong Generation, ushort ItemId, uint Count,
        float X, float Y, float Z, float VelocityX, float VelocityY, float VelocityZ,
        bool Grounded, float SleepTimer);

    public static Pose[] Capture(object module, object ecs, MethodInfo get, object[] entities)
    {
        var host = module.GetType().GetField("_managedApis", Flags)!.GetValue(module)!;
        var registry = host.GetType().GetField("WorldItems", Flags)!.GetValue(host)!;
        var published = (IDictionary)registry.GetType().GetField("_items", Flags)!.GetValue(registry)!;
        var poses = new List<Pose>(entities.Length);
        foreach (var entity in entities)
        {
            object?[] arguments = [entity, null];
            if (!(bool)get.Invoke(ecs, arguments)!) throw new InvalidOperationException("Fixture item disappeared");
            var state = arguments[1]!;
            T Field<T>(string name) => (T)state.GetType().GetField(name)!.GetValue(state)!;
            var id = (ulong)entity.GetType().GetProperty("Id")!.GetValue(entity)!;
            var latest = published[id] ?? throw new InvalidOperationException("Fixture item absent from registry");
            var generation = (ulong)latest.GetType().GetField("Generation")!.GetValue(latest)!;
            var pose = new Pose(id, generation, Field<ushort>("ItemId"), Field<uint>("Count"),
                Field<float>("X"), Field<float>("Y"), Field<float>("Z"),
                Field<float>("VelocityX"), Field<float>("VelocityY"), Field<float>("VelocityZ"),
                Field<bool>("Grounded"), Field<float>("SleepTimer"));
            if (generation == 0 || pose.Count != 1 || !float.IsFinite(pose.X) ||
                !float.IsFinite(pose.Y) || !float.IsFinite(pose.Z))
                throw new InvalidOperationException("Invalid fixture identity/count/position");
            poses.Add(pose);
        }
        return poses.OrderBy(pose => pose.EntityId).ToArray();
    }

    public static void Write(string phase, Pose[] poses) =>
        Console.WriteLine("authority_item_poses=" + JsonSerializer.Serialize(new { phase, poses }));
}
