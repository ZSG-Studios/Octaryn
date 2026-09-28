using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;

internal static class PlayerSpawnProbe
{
    public static void Run(string bundle, string world, string mode)
    {
        bundle = Path.GetFullPath(bundle);
        world = Path.GetFullPath(world);
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        AssemblyLoadContext.Default.Resolving += (_, name) =>
            AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, name.Name + ".dll"));
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Server.dll"));
        var moduleType = assembly.GetType("Octaryn.Server.Modules.ModuleActivator", true)!;
        var savePath = Path.Combine(world, "player_1.json");
        var original = File.Exists(savePath) ? File.ReadAllBytes(savePath) : null;
        using var module = (IDisposable)Activator.CreateInstance(moduleType)!;
        var sink = Activator.CreateInstance(assembly.GetType("Octaryn.Server.Host.ConsoleCommandSink", true)!)!;
        if ((int)moduleType.GetMethod("Activate", flags)!.Invoke(module, [sink])! != 0)
            throw new Exception("Module activation failed");
        object Snapshot() => moduleType.GetMethod("SnapshotPlayer", flags)!.Invoke(module, null)!;
        float Value(object state, string name) => (float)state.GetType().GetProperty(name)!.GetValue(state)!;
        var expected = mode == "saved" ? new[] { 96f, 3f, -3f, -.1f, 1.2f } : new[] { 0f, 3f, -9f, -.15f, .6f };
        var fields = new[] { "X", "Y", "Z", "Pitch", "Yaw" };
        void CheckPose(object state, bool allowGravity)
        {
            for (int i = 0; i < fields.Length; ++i)
            {
                var value = Value(state, fields[i]);
                if (!float.IsFinite(value) || MathF.Abs(value - expected[i]) > (allowGravity && i == 1 ? .02f : .00001f))
                    throw new Exception($"{mode} {fields[i]} changed unexpectedly: {value} != {expected[i]}");
            }
        }
        CheckPose(Snapshot(), false);
        if (mode == "saved" && !File.ReadAllBytes(savePath).SequenceEqual(original!))
            throw new Exception("Activation rewrote the valid saved pose");
        var controller = moduleType.GetField("_playerController", flags)!.GetValue(module)!;
        var ready = (Func<bool>)controller.GetType().GetField("_collisionReady", flags)!.GetValue(controller)!;
        var firstReady = ready();
        if (mode == "saved" && firstReady) throw new Exception("Fixture failed to exercise missing collision at the saved location");
        var wait = Stopwatch.StartNew();
        while (!ready())
        {
            CheckPose(Snapshot(), false);
            if (wait.Elapsed.TotalSeconds > 10) throw new Exception("Saved-location collision never became ready");
            Thread.Sleep(1);
        }
        CheckPose(Snapshot(), false);
        var policy = assembly.GetType("Octaryn.Server.Host.NativeHostPolicyLibrary", true)!;
        var startup = policy.GetMethod("CreateStartupFrame", flags)!.Invoke(null, null)!;
        moduleType.GetMethod("Tick", flags)!.Invoke(module, [startup]);
        CheckPose(Snapshot(), true);
        Console.WriteLine(JsonSerializer.Serialize(new { status = "passed", mode,
            savedPoseRetained = mode == "saved", firstCollisionReady = firstReady,
            collisionReady = true, activationPose = expected, startupTickChecked = true }));
    }
}
