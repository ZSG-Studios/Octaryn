using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal sealed class ClientBlockRenderRules
{
    private readonly ClientBlockRenderCatalog _catalog;

    public ClientBlockRenderRules()
        : this(ClientBlockRenderCatalog.LoadBundledCatalog())
    {
    }

    public ClientBlockRenderRules(ClientBlockRenderCatalog catalog)
    {
        _catalog = catalog;
    }

    public ClientBlockRenderProperties Properties(BlockId block)
    {
        return _catalog.Properties(block);
    }

    public ClientBlockRenderKind RenderKind(BlockId block)
    {
        return Properties(block).Kind;
    }

    public bool ShouldBuildCubeFaces(BlockId block)
    {
        return RenderKind(block) is ClientBlockRenderKind.OpaqueCube or ClientBlockRenderKind.TransparentCube;
    }

    public ReadOnlySpan<Direction> SpriteFaceDirections(BlockId block)
    {
        return Properties(block).RequiresSolidBase ? AllSpriteDirections : CrossSpriteDirections;
    }

    public int AtlasLayer(BlockId block, Direction direction)
    {
        return _catalog.AtlasLayer(block, direction);
    }

    public bool IsCubeFaceVisible(BlockId block, BlockId neighbor)
    {
        if (!ShouldBuildCubeFaces(block))
        {
            return false;
        }

        if (neighbor == BlockId.Air)
        {
            return true;
        }

        var blockProperties = Properties(block);
        var neighborProperties = Properties(neighbor);
        if (blockProperties.Kind == ClientBlockRenderKind.TransparentCube)
        {
            return neighbor.Value != block.Value && !neighborProperties.HasOcclusion;
        }

        if (neighborProperties.IsSprite)
        {
            return true;
        }

        return blockProperties.IsOpaque && !neighborProperties.HasOcclusion;
    }

    public IEnumerable<Direction> VisibleCubeFaces(
        ClientChunkNeighborhoodSnapshot snapshot,
        int blockX,
        int blockY,
        int blockZ)
    {
        var block = snapshot.LocalBlock(1, 1, blockX, blockY, blockZ);
        if (IsCubeFaceVisible(block, snapshot.NeighborhoodBlock(blockX, blockY, blockZ, 0, 0, 1)))
        {
            yield return Direction.PositiveZ;
        }

        if (IsCubeFaceVisible(block, snapshot.NeighborhoodBlock(blockX, blockY, blockZ, 0, 0, -1)))
        {
            yield return Direction.NegativeZ;
        }

        if (IsCubeFaceVisible(block, snapshot.NeighborhoodBlock(blockX, blockY, blockZ, 1, 0, 0)))
        {
            yield return Direction.PositiveX;
        }

        if (IsCubeFaceVisible(block, snapshot.NeighborhoodBlock(blockX, blockY, blockZ, -1, 0, 0)))
        {
            yield return Direction.NegativeX;
        }

        if (IsCubeFaceVisible(block, snapshot.NeighborhoodBlock(blockX, blockY, blockZ, 0, 1, 0)))
        {
            yield return Direction.PositiveY;
        }

        if (IsCubeFaceVisible(block, snapshot.NeighborhoodBlock(blockX, blockY, blockZ, 0, -1, 0)))
        {
            yield return Direction.NegativeY;
        }
    }

    private static ReadOnlySpan<Direction> CrossSpriteDirections =>
    [
        Direction.PositiveZ,
        Direction.NegativeZ,
        Direction.PositiveX,
        Direction.NegativeX
    ];

    private static ReadOnlySpan<Direction> AllSpriteDirections =>
    [
        Direction.PositiveZ,
        Direction.NegativeZ,
        Direction.PositiveX,
        Direction.NegativeX,
        Direction.PositiveY,
        Direction.NegativeY
    ];
}
