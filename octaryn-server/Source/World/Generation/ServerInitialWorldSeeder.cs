using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.World.Generation;

internal static class ServerInitialWorldSeeder
{
    public const int SpawnChunkOriginX = 0;
    public const int SpawnChunkOriginZ = 0;
    private const int UnseededWorldMaxBlockCount = 1;

    public static bool ShouldSeedSpawnChunkColumn(ServerBlockStore blocks)
    {
        return blocks.BlockCount <= UnseededWorldMaxBlockCount;
    }

    public static int SeedSpawnChunkColumn(ServerTerrainGenerator terrainGenerator, ServerBlockStore blocks)
    {
        var edits = terrainGenerator.GenerateChunkColumn(SpawnChunkOriginX, SpawnChunkOriginZ);
        Octaryn.Server.ServerLiveDebugLog.Write($"server_live_chunk_generate origin=({SpawnChunkOriginX},{SpawnChunkOriginZ}) edits={edits.Count}");
        foreach (var edit in edits)
        {
            blocks.SetBlock(edit);
        }

        return edits.Count;
    }
}
