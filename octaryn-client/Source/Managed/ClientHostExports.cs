using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Client.ClientHost;
using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;

namespace Octaryn.Client;

internal static class ClientHostExports
{
    private static BasegameModuleActivator? s_basegame;
    private static ClientBlockPresentationStore? s_presentationBlocks;
    private static ClientServerSnapshotConsumer? s_serverSnapshots;
    private static bool s_initialized;

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_initialize", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int Initialize(ClientNativeHostApi* nativeApi)
    {
        var commandSink = NativeHostCommandSink.Create(nativeApi);
        if (!commandSink.IsValid)
        {
            return -1;
        }

        if (s_initialized)
        {
            ShutdownCore();
        }

        s_basegame ??= new BasegameModuleActivator();
        s_presentationBlocks = new ClientBlockPresentationStore();
        s_serverSnapshots = new ClientServerSnapshotConsumer(s_presentationBlocks);
        var activateResult = s_basegame.Activate(commandSink);
        if (activateResult != 0)
        {
            return activateResult;
        }

        s_initialized = true;
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_tick", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int Tick(HostFrameSnapshot* frameSnapshot)
    {
        if (!s_initialized ||
            s_basegame is null ||
            frameSnapshot is null ||
            frameSnapshot->Version != HostFrameSnapshot.VersionValue ||
            frameSnapshot->Size != HostFrameSnapshot.SizeValue)
        {
            return -1;
        }

        if (frameSnapshot->Input.Size != HostInputSnapshot.SizeValue ||
            frameSnapshot->Timing.Size != HostFrameTimingSnapshot.SizeValue)
        {
            return -1;
        }

        s_basegame?.Tick(in *frameSnapshot);
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_apply_server_snapshot", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int ApplyServerSnapshot(ServerSnapshotHeader* snapshotHeader)
    {
        if (!s_initialized ||
            s_basegame is null ||
            s_serverSnapshots is null ||
            snapshotHeader is null ||
            snapshotHeader->Version != ServerSnapshotHeader.VersionValue ||
            snapshotHeader->Size != ServerSnapshotHeader.SizeValue)
        {
            return -1;
        }

        return s_serverSnapshots.Apply(snapshotHeader);
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_drain_presentation_updates", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int DrainPresentationUpdates(ReplicationChange* changes, uint capacity, uint* written)
    {
        if (!s_initialized ||
            s_presentationBlocks is null ||
            written is null ||
            (capacity > 0 && changes is null))
        {
            return -1;
        }

        *written = 0;
        while (*written < capacity && s_presentationBlocks.TryDequeueUpdate(out var update))
        {
            changes[*written] = new BlockReplicationChange(update.Position, update.Block).ToReplicationChange(0);
            (*written)++;
        }

        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_shutdown", CallConvs = [typeof(CallConvCdecl)])]
    public static void Shutdown()
    {
        ShutdownCore();
    }

    private static void ShutdownCore()
    {
        s_basegame?.Dispose();
        s_basegame = null;
        s_presentationBlocks = null;
        s_serverSnapshots = null;
        s_initialized = false;
    }
}
