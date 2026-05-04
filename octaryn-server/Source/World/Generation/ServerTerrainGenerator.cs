using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

internal sealed class ServerTerrainGenerator(IWorldGenerationRules rules)
{
    public IReadOnlyList<BlockEdit> GenerateChunkColumn(int originX, int originZ)
    {
        var blocks = new List<BlockEdit>();
        GenerateChunkColumn(originX, originZ, blocks);
        return blocks;
    }

    public IReadOnlyList<BlockEdit> GenerateVisibleChunkColumn(int originX, int originZ)
    {
        var blocks = new List<BlockEdit>();
        for (var localX = 0; localX < ServerBlockLimits.ChunkWidth; localX++)
        for (var localZ = 0; localZ < ServerBlockLimits.ChunkDepth; localZ++)
        {
            var worldX = originX + localX;
            var worldZ = originZ + localZ;
            var column = PlanColumn(worldX, worldZ, localX, localZ);
            AddVisibleColumnBlocks(column, blocks);
            AddFeatureBlocks(column, worldX, worldZ, blocks);
        }

        return blocks;
    }

    public void GenerateChunkColumn(int originX, int originZ, ICollection<BlockEdit> blocks)
    {
        for (var localX = 0; localX < ServerBlockLimits.ChunkWidth; localX++)
        for (var localZ = 0; localZ < ServerBlockLimits.ChunkDepth; localZ++)
        {
            var worldX = originX + localX;
            var worldZ = originZ + localZ;
            var column = PlanColumn(worldX, worldZ, localX, localZ);
            AddColumnBlocks(column, blocks);
            AddFeatureBlocks(column, worldX, worldZ, blocks);
        }
    }

    private TerrainColumnPlan PlanColumn(int worldX, int worldZ, int localX, int localZ)
    {
        var sample = new TerrainColumnSample(
            worldX,
            worldZ,
            localX,
            localZ,
            ServerBlockLimits.ChunkWidth,
            ServerBlockLimits.ChunkDepth,
            ServerBlockLimits.WorldMaxYExclusive - 1,
            ServerTerrainNoise.SampleHeight(worldX, worldZ),
            ServerTerrainNoise.SampleLowland(worldX, worldZ),
            ServerTerrainNoise.SampleBiome(worldX, worldZ));
        return rules.PlanTerrainColumn(sample);
    }

    private void AddFeatureBlocks(TerrainColumnPlan column, int worldX, int worldZ, ICollection<BlockEdit> blocks)
    {
        var featureBlocks = new List<BlockEdit>();
        rules.AddFeatureBlocks(column, ServerTerrainNoise.SamplePlant(worldX, worldZ), featureBlocks);
        foreach (var edit in featureBlocks)
        {
            AddIfValid(blocks, edit);
        }
    }

    private void AddColumnBlocks(TerrainColumnPlan column, ICollection<BlockEdit> blocks)
    {
        var fillTopExclusive = Math.Min(column.TerrainHeight, ServerBlockLimits.WorldMaxYExclusive);
        for (var y = ServerBlockLimits.WorldMinY; y < fillTopExclusive; y++)
        {
            AddIfValid(blocks, new BlockEdit(new BlockPosition(column.WorldX, y, column.WorldZ), column.FillBlock));
        }

        AddIfValid(blocks, new BlockEdit(
            new BlockPosition(column.WorldX, column.TerrainHeight, column.WorldZ),
            column.SurfaceBlock));

        var waterTopExclusive = Math.Min(rules.WaterHeight, ServerBlockLimits.WorldMaxYExclusive);
        for (var y = column.TerrainHeight; y < waterTopExclusive; y++)
        {
            AddIfValid(blocks, new BlockEdit(
                new BlockPosition(column.WorldX, y, column.WorldZ),
                rules.WaterBlock));
        }
    }

    private void AddVisibleColumnBlocks(TerrainColumnPlan column, ICollection<BlockEdit> blocks)
    {
        AddIfValid(blocks, new BlockEdit(
            new BlockPosition(column.WorldX, column.TerrainHeight, column.WorldZ),
            column.SurfaceBlock));

        var lowestNeighborHeight = LowestCardinalNeighborTerrainHeight(column.WorldX, column.WorldZ);
        var fillStartY = Math.Max(ServerBlockLimits.WorldMinY, lowestNeighborHeight + 1);
        var fillTopExclusive = Math.Min(column.TerrainHeight, ServerBlockLimits.WorldMaxYExclusive);
        for (var y = fillStartY; y < fillTopExclusive; y++)
        {
            AddIfValid(blocks, new BlockEdit(new BlockPosition(column.WorldX, y, column.WorldZ), column.FillBlock));
        }

        var waterTopExclusive = Math.Min(rules.WaterHeight, ServerBlockLimits.WorldMaxYExclusive);
        for (var y = column.TerrainHeight; y < waterTopExclusive; y++)
        {
            AddIfValid(blocks, new BlockEdit(
                new BlockPosition(column.WorldX, y, column.WorldZ),
                rules.WaterBlock));
        }
    }

    private int LowestCardinalNeighborTerrainHeight(int worldX, int worldZ)
    {
        return Math.Min(
            Math.Min(PlanColumn(worldX - 1, worldZ, 0, 0).TerrainHeight,
                PlanColumn(worldX + 1, worldZ, 0, 0).TerrainHeight),
            Math.Min(PlanColumn(worldX, worldZ - 1, 0, 0).TerrainHeight,
                PlanColumn(worldX, worldZ + 1, 0, 0).TerrainHeight));
    }

    private static void AddIfValid(ICollection<BlockEdit> blocks, BlockEdit edit)
    {
        if (ServerBlockStore.IsValidPosition(edit.Position))
        {
            blocks.Add(edit);
        }
    }
}
