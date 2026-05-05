using System.Globalization;
using Octaryn.Server.Persistence.Players;
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
        if (!Directory.Exists(worldRoot))
        {
            return 0;
        }

        HashSet<int> playerIds = [];
        foreach (var path in Directory.EnumerateFiles(worldRoot, "player_*.json"))
        {
            var name = Path.GetFileNameWithoutExtension(path);
            if (name.Length <= "player_".Length ||
                !int.TryParse(name["player_".Length..], NumberStyles.Integer, CultureInfo.InvariantCulture, out var playerId) ||
                !PlayerSaveFile.TryLoad(path, out _))
            {
                continue;
            }

            playerIds.Add(playerId);
        }

        return playerIds.Count;
    }

    private static int CountChunkOverrides(string worldRoot)
    {
        var chunkColumnCount = ChunkColumnOverrideStore.CountFiles(worldRoot);
        if (chunkColumnCount > 0)
        {
            return chunkColumnCount;
        }

        var worldBlocksPath = Path.Combine(worldRoot, "world_blocks.json");
        return WorldBlockOverrideFile.TryLoad(worldBlocksPath, out var file)
            ? ChunkColumnOverrideStore.CountColumns(file.ToEdits().ToArray())
            : 0;
    }
}
