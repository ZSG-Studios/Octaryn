using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class ServerBlockStore
{
    private readonly Dictionary<ChunkPosition, ServerChunkBlocks> _chunks = [];

    public int BlockCount => _chunks.Sum(entry => entry.Value.BlockCount);

    public BlockId GetBlock(BlockPosition position)
    {
        return IsValidPosition(position) && _chunks.TryGetValue(ChunkPositionFor(position), out var chunk)
            ? chunk.GetLocalBlock(LocalPositionFor(position))
            : BlockId.Air;
    }

    public ServerBlockEditResult SetBlock(BlockEdit edit)
    {
        if (!IsValidPosition(edit.Position))
        {
            return default;
        }

        var chunkPosition = ChunkPositionFor(edit.Position);
        if (!_chunks.TryGetValue(chunkPosition, out var chunk))
        {
            if (edit.Block == BlockId.Air)
            {
                return ServerBlockEditResult.Unchanged;
            }

            chunk = new ServerChunkBlocks();
            _chunks[chunkPosition] = chunk;
        }

        var result = chunk.SetLocalBlock(LocalPositionFor(edit.Position), edit.Block);
        if (chunk.IsEmpty)
        {
            _chunks.Remove(chunkPosition);
        }

        return result.Changed ? ServerBlockEditResult.ChangedEdit(edit) : ServerBlockEditResult.Unchanged;
    }

    public IReadOnlyList<BlockEdit> Snapshot()
    {
        return _chunks
            .OrderBy(entry => entry.Key.X)
            .ThenBy(entry => entry.Key.Y)
            .ThenBy(entry => entry.Key.Z)
            .SelectMany(entry => entry.Value.Snapshot(entry.Key))
            .ToArray();
    }

    public IReadOnlyList<BlockEdit> SnapshotChunkColumn(int originX, int originZ)
    {
        var maxXExclusive = originX + ServerBlockLimits.ChunkWidth;
        var maxZExclusive = originZ + ServerBlockLimits.ChunkDepth;
        return Snapshot()
            .Where(edit =>
                edit.Position.X >= originX &&
                edit.Position.X < maxXExclusive &&
                edit.Position.Z >= originZ &&
                edit.Position.Z < maxZExclusive)
            .ToArray();
    }

    public void Load(IEnumerable<BlockEdit> edits)
    {
        _chunks.Clear();
        foreach (var edit in edits)
        {
            SetBlock(edit);
        }
    }

    public static bool IsValidPosition(BlockPosition position)
    {
        return position.Y >= ServerBlockLimits.WorldMinY && position.Y < ServerBlockLimits.WorldMaxYExclusive;
    }

    public static ChunkPosition ChunkPositionFor(BlockPosition position)
    {
        return new ChunkPosition(
            FloorDiv(position.X, ServerBlockLimits.ChunkWidth),
            FloorDiv(position.Y, ServerBlockLimits.ChunkSectionHeight),
            FloorDiv(position.Z, ServerBlockLimits.ChunkDepth));
    }

    public static BlockPosition LocalPositionFor(BlockPosition position)
    {
        return new BlockPosition(
            FloorMod(position.X, ServerBlockLimits.ChunkWidth),
            FloorMod(position.Y, ServerBlockLimits.ChunkSectionHeight),
            FloorMod(position.Z, ServerBlockLimits.ChunkDepth));
    }

    private static int FloorDiv(int value, int divisor)
    {
        var quotient = value / divisor;
        var remainder = value % divisor;
        return remainder < 0 ? quotient - 1 : quotient;
    }

    private static int FloorMod(int value, int divisor)
    {
        var result = value % divisor;
        return result < 0 ? result + divisor : result;
    }
}
