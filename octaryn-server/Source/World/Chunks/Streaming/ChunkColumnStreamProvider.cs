using System.Runtime.InteropServices;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Networking;
using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Chunks;

internal sealed class ChunkColumnStreamProvider
{
    private readonly BlockStore _blocks;
    private readonly bool _hasGeneratedTerrain;

    public ChunkColumnStreamProvider(
        BlockStore blocks,
        bool hasGeneratedTerrain)
    {
        _blocks = blocks;
        _hasGeneratedTerrain = hasGeneratedTerrain;
    }

    public unsafe int RequestChunkColumns(ChunkColumnRequestFrame* requestFrame)
    {
        var result = NativeBlockStoreLibrary.ChunkStreamRequestColumnsIfAvailable(
            _blocks.NativeHandle,
            _hasGeneratedTerrain ? 1u : 0u,
            requestFrame);
        if (result == 0)
        {
            LiveDebugLog.Write($"server_live_chunk_request center=({requestFrame->CenterChunkX},{requestFrame->CenterChunkZ}) radius={requestFrame->Radius} columns={requestFrame->ColumnCount} blocks={requestFrame->BlockCount}");
        }

        return result;
    }

    public unsafe NativeChunkStreamSnapshotResult WriteProcessSnapshotFile(
        IntPtr streamWriteTracker,
        string streamPath,
        NativeChunkViewIntent intent,
        NativeChunkStreamProcessWritePlan writePlan,
        bool metadataOnly,
        WorldTimeSnapshot worldTime,
        PlayerState playerState)
    {
        var streamPathPointer = Marshal.StringToCoTaskMemUTF8(streamPath);
        try
        {
            var request = new NativeChunkStreamProcessSnapshotRequest(
                streamPathPointer,
                intent,
                writePlan,
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
                NativePlayerSimulation.IsFlyControlMode(playerState.ControlMode) ? 1u : 0u,
                playerState.IsOnGround ? 1u : 0u);
            var result = default(NativeChunkStreamSnapshotResult);
            var writeResult = NativeBlockStoreLibrary.ChunkStreamWriteProcessSnapshotFile(
                _blocks.NativeHandle,
                streamWriteTracker,
                &request,
                &result);
            if (writeResult != 0)
            {
                throw new InvalidOperationException("Native chunk stream process snapshot write failed.");
            }

            return result;
        }
        finally
        {
            Marshal.FreeCoTaskMem(streamPathPointer);
        }
    }
}
