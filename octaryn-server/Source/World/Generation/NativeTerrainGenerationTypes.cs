using System.Runtime.InteropServices;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeTerrainColumnSample(
    int worldX,
    int worldZ,
    int localX,
    int localZ,
    int localWidth,
    int localDepth,
    int maxTerrainY,
    float heightNoise,
    float lowlandNoise,
    float biomeNoise)
{
    public readonly int WorldX = worldX;
    public readonly int WorldZ = worldZ;
    public readonly int LocalX = localX;
    public readonly int LocalZ = localZ;
    public readonly int LocalWidth = localWidth;
    public readonly int LocalDepth = localDepth;
    public readonly int MaxTerrainY = maxTerrainY;
    public readonly float HeightNoise = heightNoise;
    public readonly float LowlandNoise = lowlandNoise;
    public readonly float BiomeNoise = biomeNoise;

    public TerrainColumnSample ToTerrainColumnSample()
    {
        return new TerrainColumnSample(
            WorldX,
            WorldZ,
            LocalX,
            LocalZ,
            LocalWidth,
            LocalDepth,
            MaxTerrainY,
            HeightNoise,
            LowlandNoise,
            BiomeNoise);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeTerrainColumnPlan(
    int worldX,
    int worldZ,
    int localX,
    int localZ,
    int localWidth,
    int localDepth,
    int terrainHeight,
    int decorationY,
    ushort surfaceBlock,
    ushort fillBlock,
    uint isLowland,
    uint hasGrassSurface)
{
    public readonly int WorldX = worldX;
    public readonly int WorldZ = worldZ;
    public readonly int LocalX = localX;
    public readonly int LocalZ = localZ;
    public readonly int LocalWidth = localWidth;
    public readonly int LocalDepth = localDepth;
    public readonly int TerrainHeight = terrainHeight;
    public readonly int DecorationY = decorationY;
    public readonly ushort SurfaceBlock = surfaceBlock;
    public readonly ushort FillBlock = fillBlock;
    public readonly uint IsLowland = isLowland;
    public readonly uint HasGrassSurface = hasGrassSurface;

    public static NativeTerrainColumnPlan FromTerrainColumnPlan(TerrainColumnPlan plan)
    {
        return new NativeTerrainColumnPlan(
            plan.WorldX,
            plan.WorldZ,
            plan.LocalX,
            plan.LocalZ,
            plan.LocalWidth,
            plan.LocalDepth,
            plan.TerrainHeight,
            plan.DecorationY,
            plan.SurfaceBlock.Value,
            plan.FillBlock.Value,
            plan.IsLowland ? 1u : 0u,
            plan.HasGrassSurface ? 1u : 0u);
    }
}
