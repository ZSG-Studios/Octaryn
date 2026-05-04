namespace Octaryn.Shared.World;

public static class ChunkConstants
{
    public const int Width = 32;
    public const int Depth = 32;
    public const int SectionHeight = 32;
    public const int WorldHeight = 512;
    public const int WorldMinY = -WorldHeight / 2;
    public const int WorldMaxYExclusive = WorldMinY + WorldHeight;
    public const int SectionBlockCount = Width * SectionHeight * Depth;
}
