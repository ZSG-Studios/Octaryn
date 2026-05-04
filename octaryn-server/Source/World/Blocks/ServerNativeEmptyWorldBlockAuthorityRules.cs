using Octaryn.Server.World.Generation;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class ServerNativeEmptyWorldBlockAuthorityRules : IBlockAuthorityRules
{
    public static ServerNativeEmptyWorldBlockAuthorityRules Instance { get; } = new();

    private ServerNativeEmptyWorldBlockAuthorityRules()
    {
    }

    public bool IsKnownBlock(BlockId block)
    {
        return block == BlockId.Air || block == ServerNativeEmptyWorldGenerator.WhiteBlock;
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
        return block == ServerNativeEmptyWorldGenerator.WhiteBlock;
    }

    public bool IsSolidBlock(BlockId block)
    {
        return block == ServerNativeEmptyWorldGenerator.WhiteBlock;
    }
}
