namespace Octaryn.Shared.World;

public readonly record struct TerrainColumnPlan(
    int WorldX,
    int WorldZ,
    int LocalX,
    int LocalZ,
    int LocalWidth,
    int LocalDepth,
    int TerrainHeight,
    int DecorationY,
    BlockId SurfaceBlock,
    BlockId FillBlock,
    bool IsLowland,
    bool HasGrassSurface);
