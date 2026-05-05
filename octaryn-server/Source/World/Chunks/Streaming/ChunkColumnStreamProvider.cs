using Octaryn.Server;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Generation;
using Octaryn.Shared.Networking;
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

        var columnCount = CheckedColumnCount(requestFrame->Radius);
        if (requestFrame->ColumnCapacity < columnCount)
        {
            return WriteChunkColumnRequestResult(requestFrame, columnCount, 0, status: 3);
        }

        if (requestFrame->ColumnsAddress == 0 ||
            (requestFrame->BlockCapacity > 0 && requestFrame->BlocksAddress == 0))
        {
            return -1;
        }

        var stream = CaptureChunkColumns(
            requestFrame->CenterChunkX,
            requestFrame->CenterChunkZ,
            requestFrame->Radius,
            windowEpoch: 0);
        if (requestFrame->BlockCapacity < stream.Blocks.Count)
        {
            return WriteChunkColumnRequestResult(requestFrame, columnCount, (uint)stream.Blocks.Count, status: 4);
        }

        var columns = (ChunkColumnSnapshotColumn*)requestFrame->ColumnsAddress;
        for (var index = 0; index < stream.Columns.Count; index++)
        {
            var column = stream.Columns[index];
            columns[index] = new ChunkColumnSnapshotColumn(
                column.ChunkX,
                column.ChunkZ,
                column.OriginX,
                column.OriginZ,
                column.BlockOffset,
                column.BlockCount);
        }

        var blocks = (ChunkColumnSnapshotBlock*)requestFrame->BlocksAddress;
        for (var index = 0; index < stream.Blocks.Count; index++)
        {
            var block = stream.Blocks[index];
            blocks[index] = new ChunkColumnSnapshotBlock(
                block.X,
                block.Y,
                block.Z,
                block.Block);
        }

        LiveDebugLog.Write($"server_live_chunk_request center=({requestFrame->CenterChunkX},{requestFrame->CenterChunkZ}) radius={requestFrame->Radius} columns={stream.Columns.Count} blocks={stream.Blocks.Count}");
        return WriteChunkColumnRequestResult(requestFrame, (uint)stream.Columns.Count, (uint)stream.Blocks.Count, status: 0);
    }

    public unsafe ChunkColumnStream CaptureChunkColumns(
        int centerChunkX,
        int centerChunkZ,
        uint radius,
        ulong windowEpoch,
        bool hasPreviousWindow = false,
        int previousCenterChunkX = 0,
        int previousCenterChunkZ = 0,
        uint previousRadius = 0,
        bool metadataOnly = false)
    {
        if (radius > ChunkColumnStreamingLimits.MaxRequestRadius)
        {
            throw new ArgumentOutOfRangeException(nameof(radius));
        }

        var counts = default(NativeChunkStreamCounts);
        var countResult = NativeBlockStoreLibrary.ChunkStreamCount(
            _blocks.NativeHandle,
            centerChunkX,
            centerChunkZ,
            radius,
            hasPreviousWindow ? 1u : 0u,
            previousCenterChunkX,
            previousCenterChunkZ,
            previousRadius,
            metadataOnly ? 1u : 0u,
            &counts);
        if (countResult != 0)
        {
            throw new InvalidOperationException("Native chunk stream count failed.");
        }

        var nativeEvents = new NativeChunkWindowEvent[counts.EventCount];
        var nativeColumns = new NativeChunkStreamColumn[counts.ColumnCount];
        var nativeBlocks = new NativeChunkStreamBlock[counts.BlockCount];
        var written = default(NativeChunkStreamCounts);
        fixed (NativeChunkWindowEvent* eventPointer = nativeEvents)
        fixed (NativeChunkStreamColumn* columnPointer = nativeColumns)
        fixed (NativeChunkStreamBlock* blockPointer = nativeBlocks)
        {
            var fillResult = NativeBlockStoreLibrary.ChunkStreamFill(
                _blocks.NativeHandle,
                centerChunkX,
                centerChunkZ,
                radius,
                hasPreviousWindow ? 1u : 0u,
                previousCenterChunkX,
                previousCenterChunkZ,
                previousRadius,
                metadataOnly ? 1u : 0u,
                eventPointer,
                counts.EventCount,
                columnPointer,
                counts.ColumnCount,
                blockPointer,
                counts.BlockCount,
                &written);
            if (fillResult != 0)
            {
                throw new InvalidOperationException("Native chunk stream fill failed.");
            }
        }

        return new ChunkColumnStream(
            centerChunkX,
            centerChunkZ,
            radius,
            new ChunkWindowPlan(windowEpoch, ToWindowEvents(nativeEvents)),
            nativeColumns.Select(ToStreamColumn).ToArray(),
            nativeBlocks.Select(ToStreamBlock).ToArray());
    }

    private static uint CheckedColumnCount(uint radius)
    {
        var width = checked(radius * 2u + 1u);
        return checked(width * width);
    }

    private static ChunkWindowEvent ToWindowEvent(NativeChunkWindowEvent nativeEvent)
    {
        return new ChunkWindowEvent(
            (ChunkWindowEventKind)nativeEvent.Kind,
            nativeEvent.ChunkX,
            nativeEvent.ChunkZ);
    }

    private static IReadOnlyList<ChunkWindowEvent> ToWindowEvents(NativeChunkWindowEvent[] nativeEvents)
    {
        return nativeEvents.Select(ToWindowEvent).ToArray();
    }

    private static ChunkColumnStreamColumn ToStreamColumn(NativeChunkStreamColumn column)
    {
        return new ChunkColumnStreamColumn(
            column.ChunkX,
            column.ChunkZ,
            column.OriginX,
            column.OriginZ,
            column.BlockOffset,
            column.BlockCount);
    }

    private static ChunkColumnStreamBlock ToStreamBlock(NativeChunkStreamBlock block)
    {
        return new ChunkColumnStreamBlock(block.X, block.Y, block.Z, block.Block);
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
