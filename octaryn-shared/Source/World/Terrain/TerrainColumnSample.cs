namespace Octaryn.Shared.World;

public readonly record struct TerrainColumnSample(
    int WorldX,
    int WorldZ,
    int LocalX,
    int LocalZ,
    int LocalWidth,
    int LocalDepth,
    int MaxTerrainY,
    float HeightNoise,
    float LowlandNoise,
    float BiomeNoise);
