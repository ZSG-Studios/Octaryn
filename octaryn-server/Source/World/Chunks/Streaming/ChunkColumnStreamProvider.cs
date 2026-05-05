using System.Runtime.InteropServices;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Generation;
using Octaryn.Shared.Networking;
using Octaryn.Shared.Time;
using Octaryn.Shared.World;

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
        if (requestFrame is null ||
            requestFrame->Version != ChunkColumnRequestFrame.VersionValue ||
            requestFrame->Size != ChunkColumnRequestFrame.SizeValue)
        {
            return -1;
        }

        if (_terrainGenerator is null && _nativeEmptyWorldGenerator is null)
        {
            return WriteChunkColumnRequestResult(requestFrame, 0, 0, status: 5);
        }

        if (requestFrame->Radius > ChunkColumnStreamingLimits.MaxRequestRadius)
        {
            return WriteChunkColumnRequestResult(requestFrame, 0, 0, status: 2);
        }

        var counts = default(NativeChunkStreamCounts);
        var countResult = NativeBlockStoreLibrary.ChunkStreamCount(
            _blocks.NativeHandle,
            requestFrame->CenterChunkX,
            requestFrame->CenterChunkZ,
            requestFrame->Radius,
            0u,
            0,
            0,
            0u,
            0u,
            &counts);
        if (countResult != 0)
        {
            return -1;
        }

        if (requestFrame->ColumnCapacity < counts.ColumnCount)
        {
            return WriteChunkColumnRequestResult(requestFrame, counts.ColumnCount, 0, status: 3);
        }

        if (requestFrame->BlockCapacity < counts.BlockCount)
        {
            return WriteChunkColumnRequestResult(requestFrame, counts.ColumnCount, counts.BlockCount, status: 4);
        }

        if (requestFrame->ColumnsAddress == 0 ||
            (requestFrame->BlockCapacity > 0 && requestFrame->BlocksAddress == 0))
        {
            return -1;
        }

        var nativeColumns = new NativeChunkStreamColumn[counts.ColumnCount];
        var nativeBlocks = new NativeChunkStreamBlock[counts.BlockCount];
        var written = default(NativeChunkStreamCounts);
        fixed (NativeChunkStreamColumn* columnPointer = nativeColumns)
        fixed (NativeChunkStreamBlock* blockPointer = nativeBlocks)
        {
            var fillResult = NativeBlockStoreLibrary.ChunkStreamFill(
                _blocks.NativeHandle,
                requestFrame->CenterChunkX,
                requestFrame->CenterChunkZ,
                requestFrame->Radius,
                0u,
                0,
                0,
                0u,
                0u,
                null,
                0u,
                columnPointer,
                counts.ColumnCount,
                blockPointer,
                counts.BlockCount,
                &written);
            if (fillResult != 0)
            {
                return -1;
            }
        }

        var columns = (ChunkColumnSnapshotColumn*)requestFrame->ColumnsAddress;
        for (var index = 0; index < written.ColumnCount; index++)
        {
            var column = nativeColumns[index];
            columns[index] = new ChunkColumnSnapshotColumn(
                column.ChunkX,
                column.ChunkZ,
                column.OriginX,
                column.OriginZ,
                column.BlockOffset,
                column.BlockCount);
        }

        var blocks = (ChunkColumnSnapshotBlock*)requestFrame->BlocksAddress;
        for (var index = 0; index < written.BlockCount; index++)
        {
            var block = nativeBlocks[index];
            blocks[index] = new ChunkColumnSnapshotBlock(
                block.X,
                block.Y,
                block.Z,
                block.Block);
        }

        LiveDebugLog.Write($"server_live_chunk_request center=({requestFrame->CenterChunkX},{requestFrame->CenterChunkZ}) radius={requestFrame->Radius} columns={written.ColumnCount} blocks={written.BlockCount}");
        return WriteChunkColumnRequestResult(requestFrame, written.ColumnCount, written.BlockCount, status: 0);
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

    private static unsafe int WriteChunkColumnRequestResult(
        ChunkColumnRequestFrame* requestFrame,
        uint columnCount,
        uint blockCount,
        uint status)
    {
        *requestFrame = new ChunkColumnRequestFrame(
            requestFrame->CenterChunkX,
            requestFrame->CenterChunkZ,
            requestFrame->Radius,
            requestFrame->ColumnCapacity,
            requestFrame->BlockCapacity,
            columnCount,
            blockCount,
            status,
            requestFrame->ColumnsAddress,
            requestFrame->BlocksAddress);
        return status == 0 ? 0 : -(int)status;
    }
}
