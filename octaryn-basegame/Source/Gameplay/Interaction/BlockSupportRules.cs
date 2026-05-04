using Octaryn.Basegame.Content.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Gameplay.Interaction;

public static class BlockSupportRules
{
    public static bool CanStaySupported(BlockId block, BlockPosition position, BlockId belowBlock)
    {
        if (block == BlockId.Air)
        {
            return true;
        }

        if (RequiresGrass(block))
        {
            return belowBlock == BlockCatalog.Grass;
        }

        if (RequiresSolidBase(block))
        {
            return IsSolid(belowBlock);
        }

        return true;
    }

    public static bool RequiresGrass(BlockId block)
    {
        return BlockCatalog.RequiresGrass(block);
    }

    public static bool RequiresSolidBase(BlockId block)
    {
        return BlockCatalog.RequiresSolidBase(block);
    }

    public static bool IsSolid(BlockId block)
    {
        return BlockCatalog.IsSolid(block);
    }
}
