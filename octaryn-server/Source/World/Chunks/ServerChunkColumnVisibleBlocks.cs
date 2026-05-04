using Octaryn.Shared.World;

namespace Octaryn.Server.World.Chunks;

internal static class ServerChunkColumnVisibleBlocks
{
    private static readonly (int X, int Y, int Z)[] NeighborOffsets =
    [
        (-1, 0, 0),
        (1, 0, 0),
        (0, -1, 0),
        (0, 1, 0),
        (0, 0, -1),
        (0, 0, 1)
    ];

    public static IReadOnlyList<BlockEdit> CullHiddenBlocks(IReadOnlyList<BlockEdit> blocks)
    {
        if (blocks.Count == 0)
        {
            return blocks;
        }

        HashSet<BlockPosition> solid = [];
        foreach (var block in blocks)
        {
            if (block.Block != BlockId.Air)
            {
                solid.Add(block.Position);
            }
        }

        List<BlockEdit> visible = [];
        foreach (var block in blocks)
        {
            if (block.Block == BlockId.Air || HasOpenFace(block.Position, solid))
            {
                visible.Add(block);
            }
        }

        return visible;
    }

    private static bool HasOpenFace(BlockPosition position, HashSet<BlockPosition> solid)
    {
        foreach (var offset in NeighborOffsets)
        {
            var neighbor = new BlockPosition(
                position.X + offset.X,
                position.Y + offset.Y,
                position.Z + offset.Z);
            if (!solid.Contains(neighbor))
            {
                return true;
            }
        }

        return false;
    }
}
