using System.Diagnostics;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

internal static unsafe partial class Program
{
    private static void VerifyLiveScene(string[] args)
    {
        string Option(string name)
        {
            var index = Array.IndexOf(args, name);
            if (index < 0 || index + 1 >= args.Length) throw new ArgumentException("Live scene probe requires " + name);
            return Path.GetFullPath(args[index + 1]);
        }
        var sceneLibrary = Option("--native-scene");
        var jobsLibrary = Option("--native-jobs");
        var root = Option("--scene-root");
        Require(File.Exists(sceneLibrary) && File.Exists(jobsLibrary) && Directory.Exists(root), "live scene fixture path unavailable");
        var assetOption = Array.IndexOf(args, "--asset-relative");
        var relativeAsset = assetOption >= 0 && assetOption + 1 < args.Length ? args[assetOption + 1] :
            File.Exists(Path.Combine(root, "Assets", "Scene", "scene-import.json")) ? "Assets/Scene/scene-import.json" : "Assets/Scene/scene.gltf";
        Environment.SetEnvironmentVariable("OCTARYN_SCENE_LOADING_LIBRARY", sceneLibrary);
        Environment.SetEnvironmentVariable("OCTARYN_NATIVE_JOBS_LIBRARY", jobsLibrary);
        var manifest = Manifest() with
        {
            RequestedHostApis = [HostApiIds.Scene],
            ContentDeclarations = [],
            AssetDeclarations = [new("openfnv.game.scene", "scene", relativeAsset)]
        };
        using var scheduler = new NativeScheduleRuntime();
        var denied = HostModuleContext.Create(manifest with { RequestedHostApis = [] }, new Host(), new Host(), root, scheduler);
        Require(denied.Scene is null, "live backend bypassed capability request");
        using var api = HostModuleContext.Create(manifest, new Host(), new Host(), root, scheduler).Scene
            ?? throw new InvalidOperationException("Live scene DLL backend unavailable.");
        using var other = HostModuleContext.Create(manifest, new Host(), new Host(), root, scheduler).Scene
            ?? throw new InvalidOperationException("Second live scene owner unavailable.");
        var watch = Stopwatch.StartNew();
        Require(api.BeginPrepare("openfnv.game.scene", out var ticket, out var error), "live scene admission failed: " + error);
        Require(!other.TryGetStatus(ticket, out _, out _), "native scene ticket crossed activation");
        var prepared = WaitPrepared(api, ticket);
        var preparedMs = watch.Elapsed.TotalMilliseconds;
        Require(prepared.Publication == HostScenePublication.Unpublished, "CPU preparation claimed GPU publication");
        Require(api.Release(ticket) && !api.TryGetStatus(ticket, out _, out _), "live release did not invalidate ticket");
        Require(api.BeginPrepare("openfnv.game.scene", out var cancelTicket, out error), "live cancel fixture admission failed: " + error);
        Require(cancelTicket != ticket, "released native ticket identity reused without generation change");
        Require(api.Cancel(cancelTicket), "live scene cancellation rejected");
        var canceled = WaitTerminal(api, cancelTicket);
        Require(canceled.Preparation == HostScenePreparation.Canceled, "live scene did not acknowledge cancellation");
        Require(api.Release(cancelTicket) && !api.Release(cancelTicket), "live canceled ticket release was not final");
        Require(other.BeginPrepare("openfnv.game.scene", out var closingTicket, out _), "live disposal fixture admission failed");
        other.Dispose();
        Require(!other.TryGetStatus(closingTicket, out _, out _) && !other.BeginPrepare("openfnv.game.scene", out _, out _),
            "disposed native owner remained accessible");
        Console.WriteLine($"host_scene_live=passed cpu_prepared=1 publication=unpublished cancel=1 release=1 owner_scope=1 disposal=1 " +
            $"prepare_ms={preparedMs:F3} retained_bytes={prepared.RetainedBytes} completed={prepared.Completed} total={prepared.Total}");
    }

    private static HostSceneProgress WaitPrepared(IHostSceneApi api, HostSceneTicket ticket)
    {
        var result = WaitTerminal(api, ticket);
        Require(result.Preparation == HostScenePreparation.CpuPrepared, "live scene did not reach CpuPrepared: " + result.Preparation);
        return result;
    }

    private static HostSceneProgress WaitTerminal(IHostSceneApi api, HostSceneTicket ticket)
    {
        var timer = Stopwatch.StartNew();
        do
        {
            Require(api.TryGetStatus(ticket, out var progress, out var error), "live scene query failed: " + error);
            if (progress.Preparation == HostScenePreparation.Failed)
                throw new InvalidOperationException("Live scene preparation failed: " + error);
            if (progress.Preparation is HostScenePreparation.CpuPrepared or HostScenePreparation.Failed or HostScenePreparation.Canceled)
                return progress;
            Thread.Sleep(1);
        } while (timer.Elapsed < TimeSpan.FromSeconds(10));
        throw new TimeoutException("Live scene preparation exceeded its 10 second watchdog.");
    }
}
