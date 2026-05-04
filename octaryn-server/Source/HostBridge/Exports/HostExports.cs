using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Server.Modules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;

namespace Octaryn.Server.HostBridge;

internal static class HostExports
{
    private static ModuleActivator? s_gameModule;
    private static NativeHostBridge s_nativeHost;
    private static bool s_initialized;

    [UnmanagedCallersOnly(EntryPoint = "octaryn_server_initialize", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int Initialize(NativeHostApi* nativeApi)
    {
        var nativeHost = NativeHostBridge.Create(nativeApi);
        if (!nativeHost.IsValid)
        {
            return -1;
        }

        if (s_initialized)
        {
            ShutdownCore();
        }

        s_gameModule ??= new ModuleActivator();
        var activateResult = s_gameModule.Activate(nativeHost);
        if (activateResult != 0)
        {
            return activateResult;
        }

        s_nativeHost = nativeHost;
        s_initialized = true;
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_server_tick", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int Tick(HostFrameSnapshot* frameSnapshot)
    {
        if (!s_initialized ||
            !s_nativeHost.IsValid ||
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

        s_gameModule?.Tick(in *frameSnapshot);
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_server_submit_client_commands", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int SubmitClientCommands(ClientCommandFrame* commandFrame)
    {
        if (!s_initialized ||
            !s_nativeHost.IsValid ||
            commandFrame is null ||
            commandFrame->Version != ClientCommandFrame.VersionValue ||
            commandFrame->Size != ClientCommandFrame.SizeValue)
        {
            return -1;
        }

        return s_gameModule?.SubmitClientCommands(
            (HostCommand*)commandFrame->CommandsAddress,
            commandFrame->CommandCount) == 0
            ? 0
            : -1;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_server_drain_server_snapshots", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int DrainServerSnapshots(ServerSnapshotHeader* snapshotHeader)
    {
        if (!s_initialized ||
            !s_nativeHost.IsValid ||
            snapshotHeader is null ||
            snapshotHeader->Version != ServerSnapshotHeader.VersionValue ||
            snapshotHeader->Size != ServerSnapshotHeader.SizeValue)
        {
            return -1;
        }

        return s_gameModule?.DrainServerSnapshots(snapshotHeader) ?? -1;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_server_request_chunk_columns", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RequestChunkColumns(ChunkColumnRequestFrame* requestFrame)
    {
        if (!s_initialized ||
            !s_nativeHost.IsValid ||
            requestFrame is null ||
            requestFrame->Version != ChunkColumnRequestFrame.VersionValue ||
            requestFrame->Size != ChunkColumnRequestFrame.SizeValue)
        {
            return -1;
        }

        return s_gameModule?.RequestChunkColumns(requestFrame) ?? -1;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_server_shutdown", CallConvs = [typeof(CallConvCdecl)])]
    public static void Shutdown()
    {
        ShutdownCore();
    }

    private static void ShutdownCore()
    {
        s_gameModule?.Dispose();
        s_gameModule = null;
        s_nativeHost = default;
        s_initialized = false;
    }
}
