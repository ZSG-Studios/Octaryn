namespace Octaryn.Shared.World;

public readonly record struct BlockInteractionTarget(
    bool HasHit,
    BlockPosition HitPosition,
    BlockPosition AdjacentPosition,
    BlockId HitBlock,
    Direction HitFace)
{
    public static BlockInteractionTarget Miss => default;
}
