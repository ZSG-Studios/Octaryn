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

    public TerrainColumnPlan PlanTerrainColumn(TerrainColumnSample sample)
    {
        var height = (float)global::System.Math.Pow(global::System.Math.Max(sample.HeightNoise * 50.0f, 0.0f), 1.3f) + 30.0f;
        height = (float)global::System.Math.Clamp(height, 0.0f, sample.MaxTerrainY);

        var isLowland = false;
        if (height < 40.0f)
        {
            height += sample.LowlandNoise * 12.0f;
            isLowland = true;
        }

        var terrainHeight = (int)global::System.Math.Ceiling(height);
        var materials = ClassifyMaterials(height, sample.BiomeNoise);
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
            isLowland,
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

    private static TerrainMaterials ClassifyMaterials(float height, float biome)
    {
        if (height + biome < 31.0f)
        {
            return new TerrainMaterials(
                BlockCatalog.Sand,
                BlockCatalog.Sand,
                HasGrassSurface: false);
        }

        biome = global::System.Math.Clamp(biome * 8.0f, -5.0f, 5.0f);
        if (height + biome < 61.0f)
        {
            return new TerrainMaterials(
                BlockCatalog.Grass,
                BlockCatalog.Dirt,
                HasGrassSurface: true);
        }

        if (height + biome < 132.0f)
        {
            return new TerrainMaterials(
                BlockCatalog.Stone,
                BlockCatalog.Stone,
                HasGrassSurface: false);
        }

        return new TerrainMaterials(
            BlockCatalog.Snow,
            BlockCatalog.Stone,
            HasGrassSurface: false);
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
