using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal readonly record struct ClientNeighborhoodBoundaryBlocks(
    BlockId BelowWorldBlock,
    BlockId MissingHorizontalChunkBlock)
{
    public static ClientNeighborhoodBoundaryBlocks Air => new(BlockId.Air, BlockId.Air);

    public static ClientNeighborhoodBoundaryBlocks StreamWindowEdge => new(new BlockId(1), new BlockId(1));
}
