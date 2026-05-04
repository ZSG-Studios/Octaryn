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

    public ChunkColumnStream CaptureChunkColumns(
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

        var window = ChunkWindow.Plan(new ChunkWindowIntent(
            windowEpoch,
            centerChunkX,
            centerChunkZ,
            radius,
            hasPreviousWindow,
            previousCenterChunkX,
            previousCenterChunkZ,
            previousRadius));

        List<ChunkColumnStreamColumn> columns = [];
        List<ChunkColumnStreamBlock> blocks = [];
        var loadedColumns = LoadedColumns(window);
        var radiusInt = (int)radius;
        for (var chunkZ = centerChunkZ - radiusInt; chunkZ <= centerChunkZ + radiusInt; chunkZ++)
        for (var chunkX = centerChunkX - radiusInt; chunkX <= centerChunkX + radiusInt; chunkX++)
        {
            var originX = checked(chunkX * ChunkConstants.Width);
            var originZ = checked(chunkZ * ChunkConstants.Depth);
            var blockOffset = (uint)blocks.Count;
            var blockCount = 0u;
            IReadOnlyList<BlockEdit> edits = metadataOnly && !loadedColumns.Contains(new ChunkWindowColumn(chunkX, chunkZ))
                ? []
                : ChunkColumnBlocks(originX, originZ);
            if (edits.Count != 0)
            {
                blockCount = (uint)edits.Count;
                foreach (var edit in edits)
                {
                    blocks.Add(new ChunkColumnStreamBlock(
                        edit.Position.X,
                        edit.Position.Y,
                        edit.Position.Z,
                        edit.Block.Value));
                }
            }

            columns.Add(new ChunkColumnStreamColumn(
                chunkX,
                chunkZ,
                originX,
                originZ,
                blockOffset,
                blockCount));
        }

        return new ChunkColumnStream(centerChunkX, centerChunkZ, radius, window, columns, blocks);
    }

    private static HashSet<ChunkWindowColumn> LoadedColumns(ChunkWindowPlan window)
    {
        return window.Events
            .Where(static @event => @event.Kind == ChunkWindowEventKind.Load)
            .Select(static @event => new ChunkWindowColumn(@event.ChunkX, @event.ChunkZ))
            .ToHashSet();
    }

    private static uint CheckedColumnCount(uint radius)
    {
        var width = checked(radius * 2u + 1u);
        return checked(width * width);
    }

    private IReadOnlyList<BlockEdit> ChunkColumnBlocks(int originX, int originZ)
    {
        return _blocks.SnapshotChunkColumn(originX, originZ);
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
