namespace Octaryn.Shared.World;

public interface IWorldGenerationRules
{
    int WaterHeight { get; }

    BlockId WaterBlock { get; }

    TerrainMaterialRules Materials { get; }

    TerrainColumnPlan PlanTerrainColumn(TerrainColumnSample sample);

    void AddFeatureBlocks(TerrainColumnPlan column, float plantNoise, ICollection<BlockEdit> blocks);
}
