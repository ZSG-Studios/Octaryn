using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldTime;

namespace Octaryn.Server.Persistence.WorldSave;

internal static class WorldSaveMetadataBuilder
{
    public static WorldSaveMetadata Build(string worldRoot)
    {
        var worldTimePath = Path.Combine(worldRoot, "world_time.json");
        var hasWorldTime = WorldTimeStore.TryLoad(worldTimePath, out _);
        var playerCount = CountPlayers(worldRoot);
        var chunkOverrideCount = CountChunkOverrides(worldRoot);
        return new WorldSaveMetadata(
            hasWorldTime || playerCount > 0 || chunkOverrideCount > 0,
            hasWorldTime,
            playerCount > 0,
            chunkOverrideCount > 0,
            playerCount,
            chunkOverrideCount);
    }

    private static int CountPlayers(string worldRoot)
    {
        return NativeWorldPersistenceLibrary.CountPlayerDirectory(worldRoot);
    }

    private static int CountChunkOverrides(string worldRoot)
    {
        var chunkColumnCount = checked((int)NativeWorldPersistenceLibrary.ScanChunkOverrideDirectory(
            worldRoot,
            string.Empty).FileCount);
        if (chunkColumnCount > 0)
        {
            return chunkColumnCount;
        }

        var worldBlocksPath = Path.Combine(worldRoot, "world_blocks.json");
        return NativeWorldPersistenceLibrary.CountWorldBlockOverrideColumns(worldBlocksPath);
    }
}
