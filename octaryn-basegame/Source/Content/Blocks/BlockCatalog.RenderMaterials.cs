using Octaryn.Shared.World;

namespace Octaryn.Basegame.Content.Blocks;

public static partial class BlockCatalog
{
    private static readonly ushort[] SpriteBlockIds =
    [
        9,
        10,
        11,
        12,
        13,
        22,
        23,
        24,
        25,
        26,
        27,
        28
    ];

    public static bool IsSprite(BlockId block)
    {
        return IndexOf(SpriteBlockIds, block.Value) >= 0;
    }

    public static BlockRenderMaterial RenderMaterial(BlockId block)
    {
        var renderPass = RenderPassFor(block);
        return new BlockRenderMaterial(
            block,
            MaterialFor(block, renderPass),
            renderPass,
            IsOccluding(block),
            (byte)SkylightOpacity(block),
            (sbyte)FluidLevel(block));
    }

    private static MaterialId MaterialFor(BlockId block, BlockRenderPass renderPass)
    {
        return renderPass == BlockRenderPass.None ? MaterialId.None : new MaterialId(block.Value);
    }

    private static BlockRenderPass RenderPassFor(BlockId block)
    {
        if (block == BlockId.Air || !IsKnown(block))
        {
            return BlockRenderPass.None;
        }

        if (IsFluid(block))
        {
            return BlockRenderPass.Fluid;
        }

        if (block == Glass)
        {
            return BlockRenderPass.Transparent;
        }

        if (IsSprite(block))
        {
            return BlockRenderPass.Cutout;
        }

        return IsOpaque(block) ? BlockRenderPass.Opaque : BlockRenderPass.None;
    }
}
