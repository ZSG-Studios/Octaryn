using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

internal static unsafe partial class Program
{
    private static HostSceneApiTable* s_sceneTable;
    private static readonly HashSet<HostSceneTicket> s_sceneTickets = [];
    private static readonly HashSet<HostSceneTicket> s_sceneCanceled = [];
    private static ulong s_sceneGeneration;
    private static int s_sceneCalls;
    private static uint s_scenePublication, s_scenePreparation = 1;

    private static void VerifyScene(string root)
    {
        Directory.CreateDirectory(Path.Combine(root, "Assets", "Scene"));
        File.WriteAllText(Path.Combine(root, "Assets", "Scene", "fixture.gltf"), "{}");
        var manifest = Manifest() with
        {
            RequestedHostApis = [HostApiIds.Scene],
            ContentDeclarations = [],
            AssetDeclarations = [new("openfnv.game.scene", "scene", "Assets/Scene/fixture.gltf"),
                new("openfnv.game.escape", "scene", "Assets/../outside.gltf"),
                new("other.module.scene", "scene", "Assets/Scene/fixture.gltf"),
                new("openfnv.game.texture", "texture", "Assets/Scene/fixture.gltf")]
        };
        Require(sizeof(HostSceneTicketNative) == 16 && sizeof(HostSceneProgressNative) == 32 && sizeof(HostSceneApiTable) == 48,
            "scene ABI sizes mismatch");
        Require(Marshal.OffsetOf<HostSceneApiTable>(nameof(HostSceneApiTable.MaximumTickets)) == 40 &&
            Marshal.OffsetOf<HostSceneProgressNative>(nameof(HostSceneProgressNative.RetainedBytes)) == 24, "scene ABI offsets mismatch");
        var provider = new NativeHostApiProvider(&SceneQueryApi);
        Require(provider.GetSceneApi(manifest, root) is null, "missing native scene API granted");
        s_sceneTable = (HostSceneApiTable*)NativeMemory.AllocZeroed((nuint)sizeof(HostSceneApiTable));
        try
        {
            *s_sceneTable = new HostSceneApiTable { Version = 1, Size = 48, BeginPrepare = &SceneBegin, Query = &SceneQuery,
                Cancel = &SceneCancel, Release = &SceneRelease, MaximumTickets = 2 };
            var denied = HostModuleContext.Create(manifest with { RequestedHostApis = [] }, new Host(), provider, root);
            Require(denied.Scene is null, "unrequested scene capability granted");
            var context = HostModuleContext.Create(manifest, new Host(), provider, root);
            using var api = context.Scene ?? throw new InvalidOperationException("requested scene API unavailable");
            foreach (var id in new[] { "openfnv.game.escape", "other.module.scene", "openfnv.game.texture", "Assets/Scene/fixture.gltf" })
                Require(!api.BeginPrepare(id, out _, out _), "undeclared scene ID granted");
            Require(api.BeginPrepare("openfnv.game.scene", out var first, out _) && first.IsValid, "scene admission failed");
            Require(api.TryGetStatus(first, out var running, out _) && running.Preparation == HostScenePreparation.Running &&
                running.Publication == HostScenePublication.Unpublished, "running CPU scene implied GPU publication");
            s_scenePreparation = 2;
            Require(api.TryGetStatus(first, out var prepared, out _) && prepared.Preparation == HostScenePreparation.CpuPrepared &&
                prepared.Publication == HostScenePublication.Unpublished, "CPU snapshot status incorrect");
            s_scenePublication = 1;
            Require(!api.TryGetStatus(first, out _, out _), "CPU-only v1 accepted GPU publication");
            s_scenePublication = 0;
            Require(api.Release(first), "scene release failed");
            var calls = s_sceneCalls;
            Require(!api.TryGetStatus(first, out _, out _) && !api.Cancel(first) && !api.Release(first) && s_sceneCalls == calls,
                "released ticket reached backend");
            Require(api.BeginPrepare("openfnv.game.scene", out var second, out _) && second.Id == first.Id && second.Generation != first.Generation,
                "reused ticket did not change generation");
            Require(!api.TryGetStatus(new(second.Id, first.Generation), out _, out _), "stale generation accepted");
            Require(api.Cancel(second) && api.TryGetStatus(second, out var canceled, out _) && canceled.Preparation == HostScenePreparation.Canceled,
                "cooperative cancellation status incorrect");
            using var other = provider.GetSceneApi(manifest, root)!;
            Require(!other.TryGetStatus(second, out _, out _), "ticket crossed activation ownership");
            Require(api.BeginPrepare("openfnv.game.scene", out _, out _), "second ticket rejected");
            calls = s_sceneCalls;
            Require(!api.BeginPrepare("openfnv.game.scene", out _, out _) && s_sceneCalls == calls, "ticket cap exceeded");
            api.Dispose();
            Require(s_sceneTickets.Count == 0 && !api.BeginPrepare("openfnv.game.scene", out _, out _), "disposal leaked live tickets");
            api.Dispose();
            s_sceneTable->MaximumTickets = 9;
            Require(provider.GetSceneApi(manifest, root) is null, "unbounded native ticket cap accepted");
            s_sceneTable->MaximumTickets = 2;
            s_sceneTable->Release = null;
            Require(provider.GetSceneApi(manifest, root) is null, "nonreleasable native scene API accepted");
            Console.WriteLine("host_scene_projection=passed declaration_scope=1 capability=1 generations=1 cancellation=1 release=1 disposal=1 bounded_tickets=1 cpu_gpu_separation=1");
        }
        finally { NativeMemory.Free(s_sceneTable); s_sceneTable = null; }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void* SceneQueryApi(uint id, uint version) => id == HostApiTableIds.Scene && version == 1 ? s_sceneTable : null;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int SceneBegin(byte* module, byte* asset, HostSceneTicketNative* ticket)
    {
        ++s_sceneCalls;
        if (Marshal.PtrToStringUTF8((nint)module) != "openfnv.game" ||
            Marshal.PtrToStringUTF8((nint)asset) != "openfnv.game.scene") return -1;
        *ticket = new() { Id = 1, Generation = ++s_sceneGeneration };
        s_sceneTickets.Add(ticket->Managed);
        return 0;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int SceneQuery(byte* module, HostSceneTicketNative* ticket, HostSceneProgressNative* progress)
    {
        ++s_sceneCalls;
        if (!s_sceneTickets.Contains(ticket->Managed)) return -1;
        *progress = new() { Preparation = s_sceneCanceled.Contains(ticket->Managed) ? 4u : s_scenePreparation,
            Publication = s_scenePublication, Completed = 3, Total = 3, RetainedBytes = 256 };
        return 0;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int SceneCancel(byte* module, HostSceneTicketNative* ticket)
    {
        ++s_sceneCalls;
        if (!s_sceneTickets.Contains(ticket->Managed)) return -1;
        s_sceneCanceled.Add(ticket->Managed);
        return 0;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int SceneRelease(byte* module, HostSceneTicketNative* ticket)
    {
        ++s_sceneCalls;
        s_sceneCanceled.Remove(ticket->Managed);
        return s_sceneTickets.Remove(ticket->Managed) ? 0 : -1;
    }
}
