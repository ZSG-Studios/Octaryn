namespace Octaryn.Server.World.Blocks;

using Octaryn.Shared.World;

internal static class BlockLimits
{
    public const int ChunkWidth = ChunkConstants.Width;
    public const int ChunkDepth = ChunkConstants.Depth;
    public const int ChunkSectionHeight = ChunkConstants.SectionHeight;
    public const int WorldHeight = ChunkConstants.WorldHeight;
    public const int WorldMinY = ChunkConstants.WorldMinY;
    public const int WorldMaxYExclusive = ChunkConstants.WorldMaxYExclusive;
}
