using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class BlockStore
{
    private readonly Dictionary<ChunkPosition, ChunkBlocks> _chunks = [];

    public int BlockCount => _chunks.Sum(entry => entry.Value.BlockCount);

    public BlockId GetBlock(BlockPosition position)
    {
        return IsValidPosition(position) && _chunks.TryGetValue(ChunkPositionFor(position), out var chunk)
            ? chunk.GetLocalBlock(LocalPositionFor(position))
            : BlockId.Air;
    }

    public bool TryGetBlock(BlockPosition position, out BlockId block)
    {
        block = BlockId.Air;
        return IsValidPosition(position) &&
            _chunks.TryGetValue(ChunkPositionFor(position), out var chunk) &&
            chunk.TryGetLocalBlock(LocalPositionFor(position), out block);
    }

    public BlockEditResult ClearBlockOverride(BlockPosition position)
    {
        if (!IsValidPosition(position) || !_chunks.TryGetValue(ChunkPositionFor(position), out var chunk))
        {
            return BlockEditResult.Unchanged;
        }

        var result = chunk.ClearLocalBlockOverride(LocalPositionFor(position));
        if (chunk.IsEmpty)
        {
            _chunks.Remove(ChunkPositionFor(position));
        }

        return result.Changed ? BlockEditResult.ChangedEdit(new BlockEdit(position, BlockId.Air)) : result;
    }

    public BlockEditResult SetBlock(BlockEdit edit, bool preserveAirOverride = false)
    {
        if (!IsValidPosition(edit.Position))
        {
            return default;
        }

        var chunkPosition = ChunkPositionFor(edit.Position);
        if (!_chunks.TryGetValue(chunkPosition, out var chunk))
        {
            if (edit.Block == BlockId.Air && !preserveAirOverride)
            {
                return BlockEditResult.Unchanged;
            }

            chunk = new ChunkBlocks();
            _chunks[chunkPosition] = chunk;
        }

        var result = chunk.SetLocalBlock(LocalPositionFor(edit.Position), edit.Block, preserveAirOverride);
        if (chunk.IsEmpty)
        {
            _chunks.Remove(chunkPosition);
        }

        return result.Changed ? BlockEditResult.ChangedEdit(edit) : BlockEditResult.Unchanged;
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
        var maxXExclusive = originX + BlockLimits.ChunkWidth;
        var maxZExclusive = originZ + BlockLimits.ChunkDepth;
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
            SetBlock(edit, preserveAirOverride: edit.Block == BlockId.Air);
        }
    }

    public int ClearOverridesMatching(Func<BlockPosition, BlockId> generatedBlocks)
    {
        var cleared = 0;
        foreach (var edit in Snapshot())
        {
            if (edit.Block != generatedBlocks(edit.Position))
            {
                continue;
            }

            if (ClearBlockOverride(edit.Position).Changed)
            {
                cleared++;
            }
        }

        return cleared;
    }

    public static bool IsValidPosition(BlockPosition position)
    {
        return position.Y >= BlockLimits.WorldMinY && position.Y < BlockLimits.WorldMaxYExclusive;
    }

    public static ChunkPosition ChunkPositionFor(BlockPosition position)
    {
        return new ChunkPosition(
            FloorDiv(position.X, BlockLimits.ChunkWidth),
            FloorDiv(position.Y, BlockLimits.ChunkSectionHeight),
            FloorDiv(position.Z, BlockLimits.ChunkDepth));
    }

    public static BlockPosition LocalPositionFor(BlockPosition position)
    {
        return new BlockPosition(
            FloorMod(position.X, BlockLimits.ChunkWidth),
            FloorMod(position.Y, BlockLimits.ChunkSectionHeight),
            FloorMod(position.Z, BlockLimits.ChunkDepth));
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
