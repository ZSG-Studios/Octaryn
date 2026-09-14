namespace Octaryn.Shared.World;

public readonly record struct BlockRenderMaterial(
    BlockId Block,
    MaterialId Material,
    BlockRenderPass RenderPass,
    bool EmitsOccluder,
    byte SkylightOpacity,
    sbyte FluidLevel);
