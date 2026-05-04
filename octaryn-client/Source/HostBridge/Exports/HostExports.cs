using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Client.Host;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;

namespace Octaryn.Client.HostBridge;

internal static class HostExports
{
    private static GameModuleActivator? s_gameModule;
    private static bool s_initialized;
    private static bool s_gameModulesDisabled;

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_initialize", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int Initialize(NativeHostApi* nativeApi)
    {
        var commandSink = HostCommandSink.Create(nativeApi);
        if (!commandSink.IsValid)
        {
            return -1;
        }

        if (s_initialized)
        {
            ShutdownCore();
        }

        s_gameModulesDisabled = AreGameModulesDisabled();
        if (!s_gameModulesDisabled)
        {
            s_gameModule ??= new GameModuleActivator();
        }

        if (s_gameModule is not null)
        {
            var activateResult = s_gameModule.Activate(commandSink);
            if (activateResult != 0)
            {
                return activateResult;
            }
        }

        s_initialized = true;
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_tick", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int Tick(HostFrameSnapshot* frameSnapshot)
    {
        if (!s_initialized ||
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

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_apply_server_snapshot", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int ApplyServerSnapshot(ServerSnapshotHeader* snapshotHeader)
    {
        if (!s_initialized ||
            snapshotHeader is null ||
            snapshotHeader->Version != ServerSnapshotHeader.VersionValue ||
            snapshotHeader->Size != ServerSnapshotHeader.SizeValue)
        {
            return -1;
        }

        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_drain_presentation_updates", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int DrainPresentationUpdates(ReplicationChange* changes, uint capacity, uint* written)
    {
        if (!s_initialized ||
            written is null ||
            (capacity > 0 && changes is null))
        {
            return -1;
        }

        *written = 0;
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_drain_chunk_mesh_uploads", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int DrainChunkMeshUploads(
        void* uploads,
        uint uploadCapacity,
        uint* uploadWritten,
        void* opaqueFaces,
        uint opaqueFaceCapacity,
        uint* opaqueFacesWritten,
        void* transparentFaces,
        uint transparentFaceCapacity,
        uint* transparentFacesWritten,
        void* spriteVertices,
        uint spriteVertexCapacity,
        uint* spriteVerticesWritten)
    {
        if (!s_initialized ||
            uploadWritten is null ||
            opaqueFacesWritten is null ||
            transparentFacesWritten is null ||
            spriteVerticesWritten is null ||
            (uploadCapacity > 0 && uploads is null) ||
            (opaqueFaceCapacity > 0 && opaqueFaces is null) ||
            (transparentFaceCapacity > 0 && transparentFaces is null) ||
            (spriteVertexCapacity > 0 && spriteVertices is null))
        {
            return -1;
        }

        *uploadWritten = 0;
        *opaqueFacesWritten = 0;
        *transparentFacesWritten = 0;
        *spriteVerticesWritten = 0;
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_shutdown", CallConvs = [typeof(CallConvCdecl)])]
    public static void Shutdown()
    {
        ShutdownCore();
    }

    private static void ShutdownCore()
    {
        s_gameModule?.Dispose();
        s_gameModule = null;
        s_initialized = false;
        s_gameModulesDisabled = false;
    }

    private static bool AreGameModulesDisabled()
    {
        var value = Environment.GetEnvironmentVariable("OCTARYN_CLIENT_DISABLE_GAME_MODULES");
        return !string.IsNullOrEmpty(value) && value[0] != '0';
    }
}
