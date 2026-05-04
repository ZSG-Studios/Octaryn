namespace Octaryn.Shared.World;

public interface IWorldGenerationRules
{
    int WaterHeight { get; }

    BlockId WaterBlock { get; }

    TerrainColumnPlan PlanTerrainColumn(TerrainColumnSample sample);

    void AddFeatureBlocks(TerrainColumnPlan column, float plantNoise, ICollection<BlockEdit> blocks);
}
