using Octaryn.Basegame.Content.Blocks;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Gameplay.Interaction;

public sealed class BlockAuthorityRules : IBlockAuthorityRules
{
    public bool IsKnownBlock(BlockId block)
    {
        return BlockCatalog.IsKnown(block);
    }

    public bool CanApplyEdit(BlockEdit edit, BlockId belowBlock)
    {
        return edit.Block == BlockId.Air ||
            BlockSupportRules.CanStaySupported(edit.Block, edit.Position, belowBlock);
    }

    public bool CanStaySupported(BlockId block, BlockPosition position, BlockId belowBlock)
    {
        return BlockSupportRules.CanStaySupported(block, position, belowBlock);
    }

    public bool IsClientPlaceable(BlockId block)
    {
        return PlayerBlockSelectionRules.IsPlaceable(block);
    }

    public bool IsSolidBlock(BlockId block)
    {
        return BlockCatalog.IsSolid(block);
    }
}
