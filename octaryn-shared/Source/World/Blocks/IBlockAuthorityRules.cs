namespace Octaryn.Shared.World;

public interface IBlockAuthorityRules
{
    bool IsKnownBlock(BlockId block);

    bool CanApplyEdit(BlockEdit edit, BlockId belowBlock);

    bool CanStaySupported(BlockId block, BlockPosition position, BlockId belowBlock);

    bool IsClientPlaceable(BlockId block);

    bool IsSolidBlock(BlockId block);
}
