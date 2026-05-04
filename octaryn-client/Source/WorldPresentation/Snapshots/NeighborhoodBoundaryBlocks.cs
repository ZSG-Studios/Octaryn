using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal readonly record struct NeighborhoodBoundaryBlocks(
    BlockId BelowWorldBlock,
    BlockId MissingHorizontalChunkBlock)
{
    public static NeighborhoodBoundaryBlocks Air => new(BlockId.Air, BlockId.Air);

    public static NeighborhoodBoundaryBlocks StreamWindowEdge => new(new BlockId(1), new BlockId(1));
}
