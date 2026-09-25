using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class DenyBlockAuthorityRules : IBlockAuthorityRules
{
    public static DenyBlockAuthorityRules Instance { get; } = new();

    private DenyBlockAuthorityRules()
    {
    }

    public bool IsKnownBlock(BlockId block)
    {
        return block == BlockId.Air;
    }

    public bool CanApplyEdit(BlockEdit edit, BlockId belowBlock)
    {
        _ = edit;
        _ = belowBlock;
        return false;
    }

    public bool CanStaySupported(BlockId block, BlockPosition position, BlockId belowBlock)
    {
        _ = block;
        _ = position;
        _ = belowBlock;
        return false;
    }

    public bool IsClientPlaceable(BlockId block)
    {
        _ = block;
        return false;
    }

    public bool IsSolidBlock(BlockId block)
    {
        _ = block;
        return false;
    }
}
