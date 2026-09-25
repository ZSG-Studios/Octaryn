using Octaryn.Basegame.Content.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Gameplay.Interaction;

public static class BlockReplacementRules
{
    public static bool CanBeReplacedByFluid(BlockId block)
    {
        return block == BlockCatalog.Leaves ||
            BlockSupportRules.RequiresGrass(block);
    }
}
