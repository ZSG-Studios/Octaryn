using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

internal sealed class TerrainGenerator(IWorldGenerationRules rules)
{
    public IReadOnlyList<BlockEdit> GenerateChunkColumn(int originX, int originZ)
    {
        var blocks = new List<BlockEdit>();
        GenerateChunkColumn(originX, originZ, blocks);
        return blocks;
    }

    public void GenerateChunkColumn(int originX, int originZ, ICollection<BlockEdit> blocks)
    {
        for (var localX = 0; localX < BlockLimits.ChunkWidth; localX++)
        for (var localZ = 0; localZ < BlockLimits.ChunkDepth; localZ++)
        {
            var worldX = originX + localX;
            var worldZ = originZ + localZ;
            var column = PlanColumn(worldX, worldZ, localX, localZ);
            AddColumnBlocks(column, blocks);
            AddFeatureBlocks(column, worldX, worldZ, blocks);
        }
    }

    public bool IsSolidBlock(BlockPosition position)
    {
        return GetGeneratedBlock(position) != BlockId.Air;
    }

    public BlockId GetGeneratedBlock(BlockPosition position)
    {
        if (!BlockStore.IsValidPosition(position))
        {
            return BlockId.Air;
        }

        var column = PlanColumn(
            position.X,
            position.Z,
            FloorMod(position.X, BlockLimits.ChunkWidth),
            FloorMod(position.Z, BlockLimits.ChunkDepth));
        if (position.Y < column.TerrainHeight)
        {
            return column.FillBlock;
        }

        if (position.Y == column.TerrainHeight)
        {
            return column.SurfaceBlock;
        }

        return position.Y < rules.WaterHeight ? rules.WaterBlock : BlockId.Air;
    }

    private TerrainColumnPlan PlanColumn(int worldX, int worldZ, int localX, int localZ)
    {
        var sample = new TerrainColumnSample(
            worldX,
            worldZ,
            localX,
            localZ,
            BlockLimits.ChunkWidth,
            BlockLimits.ChunkDepth,
            BlockLimits.WorldMaxYExclusive - 1,
            TerrainNoise.SampleHeight(worldX, worldZ),
            TerrainNoise.SampleLowland(worldX, worldZ),
            TerrainNoise.SampleBiome(worldX, worldZ));
        return rules.PlanTerrainColumn(sample);
    }

    private void AddFeatureBlocks(TerrainColumnPlan column, int worldX, int worldZ, ICollection<BlockEdit> blocks)
    {
        var featureBlocks = new List<BlockEdit>();
        rules.AddFeatureBlocks(column, TerrainNoise.SamplePlant(worldX, worldZ), featureBlocks);
        foreach (var edit in featureBlocks)
        {
            AddIfValid(blocks, edit);
        }
    }

    private void AddColumnBlocks(TerrainColumnPlan column, ICollection<BlockEdit> blocks)
    {
        var fillTopExclusive = Math.Min(column.TerrainHeight, BlockLimits.WorldMaxYExclusive);
        for (var y = BlockLimits.WorldMinY; y < fillTopExclusive; y++)
        {
            AddIfValid(blocks, new BlockEdit(new BlockPosition(column.WorldX, y, column.WorldZ), column.FillBlock));
        }

        AddIfValid(blocks, new BlockEdit(
            new BlockPosition(column.WorldX, column.TerrainHeight, column.WorldZ),
            column.SurfaceBlock));

        var waterTopExclusive = Math.Min(rules.WaterHeight, BlockLimits.WorldMaxYExclusive);
        for (var y = column.TerrainHeight; y < waterTopExclusive; y++)
        {
            AddIfValid(blocks, new BlockEdit(
                new BlockPosition(column.WorldX, y, column.WorldZ),
                rules.WaterBlock));
        }
    }

    private static void AddIfValid(ICollection<BlockEdit> blocks, BlockEdit edit)
    {
        if (BlockStore.IsValidPosition(edit.Position))
        {
            blocks.Add(edit);
        }
    }

    private static int FloorMod(int value, int divisor)
    {
        var result = value % divisor;
        return result < 0 ? result + divisor : result;
    }
}
