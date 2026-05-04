using Octaryn.Basegame.Content.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Gameplay.Player;

public static class PlayerBlockSelectionRules
{
    public static BlockId DefaultSelectedBlock => BlockCatalog.DefaultSelectedBlock;

    public static PlayerBlockSelectionState SelectTargetBlock(
        PlayerBlockSelectionState current,
        BlockId targetBlock)
    {
        return BlockCatalog.IsTargetable(targetBlock)
            ? new PlayerBlockSelectionState(targetBlock)
            : current;
    }

    public static PlayerBlockSelectionState ChangeSelectedBlock(
        PlayerBlockSelectionState current,
        int delta)
    {
        if (delta == 0)
        {
            return IsPlaceable(current.SelectedBlock)
                ? current
                : PlayerBlockSelectionState.Default;
        }

        var candidate = current.SelectedBlock;
        var maxAttempts = global::System.Math.Max(1, BlockCatalog.KnownBlockCount - 1);
        for (var attempt = 0; attempt < maxAttempts; attempt++)
        {
            candidate = OffsetKnownBlock(candidate, delta);
            if (IsPlaceable(candidate))
            {
                return new PlayerBlockSelectionState(candidate);
            }
        }

        return PlayerBlockSelectionState.Default;
    }

    public static bool IsPlaceable(BlockId block)
    {
        return BlockCatalog.IsPlaceable(block);
    }

    private static BlockId OffsetKnownBlock(BlockId block, int delta)
    {
        var selectableCount = BlockCatalog.KnownBlockCount - 1;
        var zeroBased = PositiveModulo((long)block.Value - 1 + delta, selectableCount);
        return new BlockId((ushort)(zeroBased + 1));
    }

    private static int PositiveModulo(long value, int divisor)
    {
        var result = value % divisor;
        return (int)(result < 0 ? result + divisor : result);
    }
}
