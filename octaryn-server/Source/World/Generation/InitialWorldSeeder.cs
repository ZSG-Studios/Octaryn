using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.World.Generation;

internal static class InitialWorldSeeder
{
    public const int SpawnChunkOriginX = 0;
    public const int SpawnChunkOriginZ = 0;
    private const int UnseededWorldMaxBlockCount = 1;

    public static bool ShouldSeedSpawnChunkColumn(BlockStore blocks)
    {
        return blocks.BlockCount <= UnseededWorldMaxBlockCount;
    }

    public static int SeedSpawnChunkColumn(TerrainGenerator terrainGenerator, BlockStore blocks)
    {
        var edits = terrainGenerator.GenerateChunkColumn(SpawnChunkOriginX, SpawnChunkOriginZ);
        Octaryn.Server.LiveDebugLog.Write($"server_live_chunk_generate origin=({SpawnChunkOriginX},{SpawnChunkOriginZ}) edits={edits.Count}");
        foreach (var edit in edits)
        {
            blocks.SetBlock(edit);
        }

        return edits.Count;
    }
}
