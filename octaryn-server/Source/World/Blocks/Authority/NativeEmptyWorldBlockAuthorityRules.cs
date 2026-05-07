using Octaryn.Server.World.Generation;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class NativeEmptyWorldBlockAuthorityRules : IBlockAuthorityRules
{
    public static NativeEmptyWorldBlockAuthorityRules Instance { get; } = new();

    private NativeEmptyWorldBlockAuthorityRules()
    {
    }

    public bool IsKnownBlock(BlockId block)
    {
        return block == BlockId.Air || block == NativeTerrainGenerationLibrary.EmptyWorldWhiteBlock;
    }

    public bool CanApplyEdit(BlockEdit edit, BlockId belowBlock)
    {
        _ = edit;
        _ = belowBlock;
        return true;
    }

    public bool CanStaySupported(BlockId block, BlockPosition position, BlockId belowBlock)
    {
        _ = block;
        _ = position;
        _ = belowBlock;
        return true;
    }

    public bool IsClientPlaceable(BlockId block)
    {
        return block == NativeTerrainGenerationLibrary.EmptyWorldWhiteBlock;
    }

    public bool IsSolidBlock(BlockId block)
    {
        return block == NativeTerrainGenerationLibrary.EmptyWorldWhiteBlock;
    }
}
