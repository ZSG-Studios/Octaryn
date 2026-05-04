using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Client.ClientHost;
using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;

namespace Octaryn.Client;

internal static class ClientHostExports
{
    private static ClientGameModuleActivator? s_gameModule;
    private static ClientBlockPresentationStore? s_presentationBlocks;
    private static ClientServerSnapshotConsumer? s_serverSnapshots;
    private static ClientChunkMeshPlanner? s_meshPlanner;
    private static ClientChunkMeshPacker? s_meshPacker;
    private static bool s_initialized;
    private static bool s_gameModulesDisabled;

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

        s_gameModulesDisabled = AreGameModulesDisabled();
        if (!s_gameModulesDisabled)
        {
            s_gameModule ??= new ClientGameModuleActivator();
        }

        s_presentationBlocks = new ClientBlockPresentationStore();
        s_serverSnapshots = new ClientServerSnapshotConsumer(s_presentationBlocks);
        var renderRules = s_gameModulesDisabled
            ? new ClientBlockRenderRules(ClientBlockRenderCatalog.Empty())
            : new ClientBlockRenderRules();
        s_meshPlanner = new ClientChunkMeshPlanner(renderRules);
        s_meshPacker = new ClientChunkMeshPacker(renderRules);
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

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_drain_chunk_mesh_uploads", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int DrainChunkMeshUploads(
        ClientChunkMeshUploadRecord* uploads,
        uint uploadCapacity,
        uint* uploadWritten,
        ulong* opaqueFaces,
        uint opaqueFaceCapacity,
        uint* opaqueFacesWritten,
        ulong* transparentFaces,
        uint transparentFaceCapacity,
        uint* transparentFacesWritten,
        uint* spriteVertices,
        uint spriteVertexCapacity,
        uint* spriteVerticesWritten)
    {
        if (!s_initialized ||
            s_presentationBlocks is null ||
            s_meshPlanner is null ||
            s_meshPacker is null ||
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

        while (*uploadWritten < uploadCapacity &&
               s_presentationBlocks.TryPeekDirtyChunk(out var chunk))
        {
            var snapshot = s_presentationBlocks.CaptureNeighborhood(
                chunk,
                ClientNeighborhoodBoundaryBlocks.StreamWindowEdge);
            var mesh = s_meshPacker.Pack(s_meshPlanner.Build(snapshot));
            if (mesh.SpriteVertices.Count % 4 != 0)
            {
                return -2;
            }

            if ((uint)mesh.OpaqueCubeFaces.Count > opaqueFaceCapacity - *opaqueFacesWritten ||
                (uint)mesh.TransparentCubeFaces.Count > transparentFaceCapacity - *transparentFacesWritten ||
                (uint)mesh.SpriteVertices.Count > spriteVertexCapacity - *spriteVerticesWritten)
            {
                break;
            }

            uploads[*uploadWritten] = ClientChunkMeshUploadRecord.Create(
                chunk,
                mesh,
                *opaqueFacesWritten,
                *transparentFacesWritten,
                *spriteVerticesWritten);

            Copy(mesh.OpaqueCubeFaces, opaqueFaces + *opaqueFacesWritten);
            Copy(mesh.TransparentCubeFaces, transparentFaces + *transparentFacesWritten);
            Copy(mesh.SpriteVertices, spriteVertices + *spriteVerticesWritten);
            *opaqueFacesWritten += checked((uint)mesh.OpaqueCubeFaces.Count);
            *transparentFacesWritten += checked((uint)mesh.TransparentCubeFaces.Count);
            *spriteVerticesWritten += checked((uint)mesh.SpriteVertices.Count);
            (*uploadWritten)++;
            s_presentationBlocks.RemoveDirtyChunk(chunk);
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
        s_gameModule?.Dispose();
        s_gameModule = null;
        s_presentationBlocks = null;
        s_serverSnapshots = null;
        s_meshPlanner = null;
        s_meshPacker = null;
        s_initialized = false;
        s_gameModulesDisabled = false;
    }

    private static bool AreGameModulesDisabled()
    {
        var value = Environment.GetEnvironmentVariable("OCTARYN_CLIENT_DISABLE_GAME_MODULES");
        return !string.IsNullOrEmpty(value) && value[0] != '0';
    }

    private static unsafe void Copy(IReadOnlyList<ulong> source, ulong* destination)
    {
        for (var index = 0; index < source.Count; index++)
        {
            destination[index] = source[index];
        }
    }

    private static unsafe void Copy(IReadOnlyList<uint> source, uint* destination)
    {
        for (var index = 0; index < source.Count; index++)
        {
            destination[index] = source[index];
        }
    }
}
