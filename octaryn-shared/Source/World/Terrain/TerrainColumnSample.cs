namespace Octaryn.Shared.World;

// Host-sampled geometry and climate; module rules only plan materials and features.
public readonly record struct TerrainColumnSample(
    int WorldX,
    int WorldZ,
    int LocalX,
    int LocalZ,
    int LocalWidth,
    int LocalDepth,
    int TerrainHeight,
    double Temperature,
    double Humidity,
    bool IsLowland);
