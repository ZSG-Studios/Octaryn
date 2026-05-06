using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

internal sealed unsafe class TerrainGenerator : IDisposable
{
    private readonly NativeTerrainMaterialRules _rules;

    public TerrainGenerator(IWorldGenerationRules rules)
    {
        _rules = new NativeTerrainMaterialRules(
            rules.WaterHeight,
            rules.WaterBlock.Value,
            rules.Materials.SandBlock.Value,
            rules.Materials.GrassBlock.Value,
            rules.Materials.DirtBlock.Value,
            rules.Materials.StoneBlock.Value,
            rules.Materials.SnowBlock.Value);
    }

    public BlockId GetGeneratedBlock(BlockPosition position)
    {
        ushort block = 0;
        var rules = _rules;
        var result = NativeTerrainGenerationLibrary.GeneratedBlock(
            position.X,
            position.Y,
            position.Z,
            &rules,
            &block);
        if (result != 0)
        {
            throw new InvalidOperationException("Native terrain generation failed.");
        }

        return new BlockId(block);
    }

    public void Dispose()
    {
        GC.SuppressFinalize(this);
    }
}
