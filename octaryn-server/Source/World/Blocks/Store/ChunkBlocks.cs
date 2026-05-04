using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class ChunkBlocks
{
    private readonly Dictionary<int, BlockId> _overrides = [];

    public BlockId GetLocalBlock(BlockPosition localPosition)
    {
        return IsValidLocalPosition(localPosition) &&
            _overrides.TryGetValue(LocalIndex(localPosition), out var block)
                ? block
                : BlockId.Air;
    }

    public bool TryGetLocalBlock(BlockPosition localPosition, out BlockId block)
    {
        block = BlockId.Air;
        return IsValidLocalPosition(localPosition) &&
            _overrides.TryGetValue(LocalIndex(localPosition), out block);
    }

    public BlockEditResult ClearLocalBlockOverride(BlockPosition localPosition)
    {
        if (!IsValidLocalPosition(localPosition))
        {
            return default;
        }

        var index = LocalIndex(localPosition);
        return _overrides.Remove(index)
            ? new BlockEditResult(Applied: true, Changed: true, Changes: [])
            : BlockEditResult.Unchanged;
    }

    public BlockEditResult SetLocalBlock(BlockPosition localPosition, BlockId block, bool preserveAirOverride = false)
    {
        if (!IsValidLocalPosition(localPosition))
        {
            return default;
        }

        var index = LocalIndex(localPosition);
        var hasExistingOverride = _overrides.TryGetValue(index, out var existing);
        var oldBlock = hasExistingOverride ? existing : BlockId.Air;
        if (oldBlock == block && !(preserveAirOverride && !hasExistingOverride))
        {
            return BlockEditResult.Unchanged;
        }

        if (block == BlockId.Air && !preserveAirOverride)
        {
            _overrides.Remove(index);
        }
        else
        {
            _overrides[index] = block;
        }

        return new BlockEditResult(Applied: true, Changed: true, Changes: []);
    }

    public IEnumerable<BlockEdit> Snapshot(ChunkPosition chunkPosition)
    {
        foreach (var entry in _overrides.OrderBy(entry => entry.Key))
        {
            var local = LocalPosition(entry.Key);
            yield return new BlockEdit(
                new BlockPosition(
                    chunkPosition.X * BlockLimits.ChunkWidth + local.X,
                    chunkPosition.Y * BlockLimits.ChunkSectionHeight + local.Y,
                    chunkPosition.Z * BlockLimits.ChunkDepth + local.Z),
                entry.Value);
        }
    }

    public bool IsEmpty => _overrides.Count == 0;

    public int BlockCount => _overrides.Count;

    private static bool IsValidLocalPosition(BlockPosition position)
    {
        return position.X >= 0 &&
            position.X < BlockLimits.ChunkWidth &&
            position.Y >= 0 &&
            position.Y < BlockLimits.ChunkSectionHeight &&
            position.Z >= 0 &&
            position.Z < BlockLimits.ChunkDepth;
    }

    private static int LocalIndex(BlockPosition position)
    {
        return position.X +
            position.Z * BlockLimits.ChunkWidth +
            position.Y * BlockLimits.ChunkWidth * BlockLimits.ChunkDepth;
    }

    private static BlockPosition LocalPosition(int index)
    {
        var layer = BlockLimits.ChunkWidth * BlockLimits.ChunkDepth;
        var y = index / layer;
        var remaining = index - y * layer;
        var z = remaining / BlockLimits.ChunkWidth;
        var x = remaining - z * BlockLimits.ChunkWidth;
        return new BlockPosition(x, y, z);
    }
}
