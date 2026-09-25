using Octaryn.Basegame.Content.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Content.Worldgen;

public sealed class WorldGenerationRules : IWorldGenerationRules
{
    public int WaterHeight => 30;

    public BlockId WaterBlock => BlockCatalog.WaterSource;

    public TerrainMaterialRules Materials => new(
        SandBlock: BlockCatalog.Sand,
        GrassBlock: BlockCatalog.Grass,
        DirtBlock: BlockCatalog.Dirt,
        StoneBlock: BlockCatalog.Stone,
        SnowBlock: BlockCatalog.Snow);

    public TerrainColumnPlan PlanTerrainColumn(TerrainColumnSample sample)
    {
        var terrainHeight = sample.TerrainHeight;
        var materials = ClassifyMaterials(sample);
        return new TerrainColumnPlan(
            sample.WorldX,
            sample.WorldZ,
            sample.LocalX,
            sample.LocalZ,
            sample.LocalWidth,
            sample.LocalDepth,
            terrainHeight,
            global::System.Math.Max(terrainHeight, WaterHeight),
            materials.SurfaceBlock,
            materials.FillBlock,
            sample.IsLowland,
            materials.HasGrassSurface);
    }

    private TerrainMaterials ClassifyMaterials(TerrainColumnSample sample)
    {
        if (sample.TerrainHeight <= WaterHeight + 2)
        {
            return new TerrainMaterials(
                BlockCatalog.Sand,
                BlockCatalog.Sand,
                HasGrassSurface: false);
        }

        var temperature = sample.Temperature - global::System.Math.Max(0, sample.TerrainHeight - 60) * 0.007;
        if (temperature < -0.38 || sample.TerrainHeight > 150)
        {
            return new TerrainMaterials(
                BlockCatalog.Snow,
                BlockCatalog.Stone,
                HasGrassSurface: false);
        }

        if (sample.Temperature > 0.18 && sample.Humidity < -0.1)
        {
            return new TerrainMaterials(
                BlockCatalog.Sand,
                BlockCatalog.Sand,
                HasGrassSurface: false);
        }

        if (sample.TerrainHeight > 105)
        {
            return new TerrainMaterials(
                BlockCatalog.Stone,
                BlockCatalog.Stone,
                HasGrassSurface: false);
        }

        return new TerrainMaterials(
            BlockCatalog.Grass,
            BlockCatalog.Dirt,
            HasGrassSurface: true);
    }

    private readonly record struct TerrainMaterials(BlockId SurfaceBlock, BlockId FillBlock, bool HasGrassSurface);
}
