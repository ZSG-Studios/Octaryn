namespace Octaryn.Shared.World;

public readonly record struct TerrainMaterialRules(
    BlockId SandBlock,
    BlockId GrassBlock,
    BlockId DirtBlock,
    BlockId StoneBlock,
    BlockId SnowBlock);
