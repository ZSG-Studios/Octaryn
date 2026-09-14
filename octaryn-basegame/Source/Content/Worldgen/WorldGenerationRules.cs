using System.Collections.Generic;
using Octaryn.Basegame.Content.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Content.Worldgen;

public sealed class WorldGenerationRules : IWorldGenerationRules
{
    private static readonly BlockId[] Flowers =
    [
        BlockCatalog.Bluebell,
        BlockCatalog.Gardenia,
        BlockCatalog.Lavender,
        BlockCatalog.Rose
    ];

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

    public void AddFeatureBlocks(TerrainColumnPlan column, float plantNoise, ICollection<BlockEdit> blocks)
    {
        if (!column.IsLowland || !column.HasGrassSurface)
        {
            return;
        }

        var plant = plantNoise * 0.5f + 0.5f;
        if (plant > 0.8f &&
            column.LocalX > 2 &&
            column.LocalX < column.LocalWidth - 2 &&
            column.LocalZ > 2 &&
            column.LocalZ < column.LocalDepth - 2)
        {
            AddTreeBlocks(column, plant, blocks);
            return;
        }

        if (plant > 0.55f)
        {
            blocks.Add(new BlockEdit(
                new BlockPosition(column.WorldX, column.DecorationY + 1, column.WorldZ),
                BlockCatalog.Bush));
            return;
        }

        if (plant > 0.52f)
        {
            var flowerIndex = global::System.Math.Max((int)(plant * 1000.0f) % Flowers.Length, 0);
            blocks.Add(new BlockEdit(
                new BlockPosition(column.WorldX, column.DecorationY + 1, column.WorldZ),
                Flowers[flowerIndex]));
        }
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

    private static void AddTreeBlocks(TerrainColumnPlan column, float plant, ICollection<BlockEdit> blocks)
    {
        var logHeight = (int)(3.0f + plant * 2.0f);
        for (var dy = 0; dy < logHeight; dy++)
        {
            blocks.Add(new BlockEdit(
                new BlockPosition(column.WorldX, column.DecorationY + dy + 1, column.WorldZ),
                BlockCatalog.Log));
        }

        for (var dx = -1; dx <= 1; dx++)
        for (var dz = -1; dz <= 1; dz++)
        for (var dy = 0; dy < 2; dy++)
        {
            if (dx == 0 && dz == 0 && dy == 0)
            {
                continue;
            }

            blocks.Add(new BlockEdit(
                new BlockPosition(column.WorldX + dx, column.DecorationY + logHeight + dy, column.WorldZ + dz),
                BlockCatalog.Leaves));
        }
    }

    private readonly record struct TerrainMaterials(BlockId SurfaceBlock, BlockId FillBlock, bool HasGrassSurface);
}
