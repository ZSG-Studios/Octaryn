namespace Octaryn.Client.WorldPresentation;

internal readonly record struct BlockRenderProperties(
    BlockRenderKind Kind,
    bool IsOpaque,
    bool HasOcclusion,
    bool IsSprite,
    bool IsFluid,
    int FluidLevel,
    bool RequiresSolidBase)
{
    public static BlockRenderProperties Air { get; } = new(
        BlockRenderKind.Empty,
        IsOpaque: false,
        HasOcclusion: false,
        IsSprite: false,
        IsFluid: false,
        FluidLevel: -1,
        RequiresSolidBase: false);
}
