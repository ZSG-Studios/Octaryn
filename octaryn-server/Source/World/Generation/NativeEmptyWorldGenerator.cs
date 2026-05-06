using Octaryn.Shared.World;
using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.World.Generation;

internal sealed unsafe class NativeEmptyWorldGenerator
{
    public static BlockId WhiteBlock => new(1);

    public BlockId GetGeneratedBlock(BlockPosition position)
    {
        return new BlockId(NativeTerrainGenerationLibrary.EmptyWorldGeneratedBlock(
            position.X,
            position.Y,
            position.Z));
    }

    public bool IsGeneratedSolidBlock(BlockPosition position)
    {
        return GetGeneratedBlock(position) != BlockId.Air;
    }

    public int ClearMatchingOverrides(BlockStore blocks)
    {
        return NativeTerrainGenerationLibrary.ClearEmptyWorldMatchingOverrides(blocks.NativeHandle);
    }
}
