using System.Runtime.InteropServices;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Generation;
using Octaryn.Shared.Networking;
using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Chunks;

internal sealed class ChunkColumnStreamProvider
{
    private readonly BlockStore _blocks;
    private readonly TerrainGenerator? _terrainGenerator;
    private readonly NativeEmptyWorldGenerator? _nativeEmptyWorldGenerator;

    public ChunkColumnStreamProvider(
        BlockStore blocks,
        TerrainGenerator? terrainGenerator,
        NativeEmptyWorldGenerator? nativeEmptyWorldGenerator)
    {
        _blocks = blocks;
        _terrainGenerator = terrainGenerator;
        _nativeEmptyWorldGenerator = nativeEmptyWorldGenerator;
    }

    public unsafe int RequestChunkColumns(ChunkColumnRequestFrame* requestFrame)
    {
        if (_terrainGenerator is null && _nativeEmptyWorldGenerator is null)
        {
            return NativeBlockStoreLibrary.ChunkStreamWriteRequestResult(requestFrame, 0, 0, 5);
        }

        var result = NativeBlockStoreLibrary.ChunkStreamRequestColumns(_blocks.NativeHandle, requestFrame);
        if (result == 0)
        {
            LiveDebugLog.Write($"server_live_chunk_request center=({requestFrame->CenterChunkX},{requestFrame->CenterChunkZ}) radius={requestFrame->Radius} columns={requestFrame->ColumnCount} blocks={requestFrame->BlockCount}");
        }

        return result;
    }

    public unsafe NativeChunkStreamSnapshotResult WriteSnapshotFile(
        string streamPath,
        ulong epoch,
        int centerChunkX,
        int centerChunkZ,
        uint radius,
        bool hasPreviousWindow,
        int previousCenterChunkX,
        int previousCenterChunkZ,
        uint previousRadius,
        bool metadataOnly,
        WorldTimeSnapshot worldTime,
        PlayerState playerState)
    {
        var streamPathPointer = Marshal.StringToCoTaskMemUTF8(streamPath);
        try
        {
            var request = new NativeChunkStreamSnapshotRequest(
                streamPathPointer,
                epoch,
                centerChunkX,
                centerChunkZ,
                radius,
                hasPreviousWindow ? 1u : 0u,
                previousCenterChunkX,
                previousCenterChunkZ,
                previousRadius,
                metadataOnly ? 1u : 0u,
                worldSeed: 0,
                worldTime.DayIndex,
                worldTime.SecondOfDay,
                worldTime.TotalWorldSeconds,
                worldTime.DayFraction,
                playerState.X,
                playerState.Y,
                playerState.Z,
                playerState.Pitch,
                playerState.Yaw,
                playerState.VelocityX,
                playerState.VelocityY,
                playerState.VelocityZ,
                playerState.ControlMode == PlayerControlMode.Fly ? 1u : 0u,
                playerState.IsOnGround ? 1u : 0u);
            var result = default(NativeChunkStreamSnapshotResult);
            var writeResult = NativeBlockStoreLibrary.ChunkStreamWriteSnapshotFile(
                _blocks.NativeHandle,
                &request,
                &result);
            if (writeResult != 0)
            {
                throw new InvalidOperationException("Native chunk stream snapshot write failed.");
            }

            return result;
        }
        finally
        {
            Marshal.FreeCoTaskMem(streamPathPointer);
        }
    }
}
