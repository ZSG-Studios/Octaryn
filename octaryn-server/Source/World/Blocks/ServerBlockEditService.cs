using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class ServerBlockEditService(
    ServerBlockStore blocks,
    IBlockAuthorityRules authorityRules,
    Func<BlockPosition, BlockId>? generatedBlocks = null)
{
    public BlockId GetBlock(BlockPosition position)
    {
        return blocks.TryGetBlock(position, out var block)
            ? block
            : GeneratedBlock(position);
    }

    public ServerBlockEditResult Apply(BlockEdit edit)
    {
        if (!CanApply(edit))
        {
            return default;
        }

        var result = ApplyOverride(edit);
        if (!result.Changed)
        {
            return result;
        }

        if (TryClearUnsupportedBlockAbove(edit, out var cascadeEdit))
        {
            return new ServerBlockEditResult(
                Applied: true,
                Changed: true,
                Changes: [edit, cascadeEdit]);
        }

        return result;
    }

    internal bool CanApply(BlockEdit edit)
    {
        if (!ServerBlockStore.IsValidPosition(edit.Position) || !authorityRules.IsKnownBlock(edit.Block))
        {
            return false;
        }

        if (edit.Block == BlockId.Air)
        {
            return true;
        }

        var belowPosition = new BlockPosition(edit.Position.X, edit.Position.Y - 1, edit.Position.Z);
        return authorityRules.CanApplyEdit(edit, GetBlock(belowPosition));
    }

    private ServerBlockEditResult ApplyOverride(BlockEdit edit)
    {
        var generatedBlock = GeneratedBlock(edit.Position);
        var hasOverride = blocks.TryGetBlock(edit.Position, out var existingOverride);
        var currentBlock = hasOverride ? existingOverride : generatedBlock;
        if (currentBlock == edit.Block)
        {
            return ServerBlockEditResult.Unchanged;
        }

        if (hasOverride && edit.Block == generatedBlock)
        {
            var cleared = blocks.ClearBlockOverride(edit.Position);
            return cleared.Changed ? ServerBlockEditResult.ChangedEdit(edit) : cleared;
        }

        return blocks.SetBlock(edit, preserveAirOverride: edit.Block == BlockId.Air && generatedBlock != BlockId.Air);
    }

    private bool TryClearUnsupportedBlockAbove(BlockEdit edit, out BlockEdit cascadeEdit)
    {
        cascadeEdit = default;
        if (edit.Position.Y + 1 >= ServerBlockLimits.WorldMaxYExclusive)
        {
            return false;
        }

        var abovePosition = new BlockPosition(edit.Position.X, edit.Position.Y + 1, edit.Position.Z);
        var aboveBlock = GetBlock(abovePosition);
        if (aboveBlock == BlockId.Air ||
            authorityRules.CanStaySupported(aboveBlock, abovePosition, edit.Block))
        {
            return false;
        }

        cascadeEdit = new BlockEdit(abovePosition, BlockId.Air);
        return ApplyOverride(cascadeEdit).Changed;
    }

    private BlockId GeneratedBlock(BlockPosition position)
    {
        return generatedBlocks?.Invoke(position) ?? BlockId.Air;
    }
}
