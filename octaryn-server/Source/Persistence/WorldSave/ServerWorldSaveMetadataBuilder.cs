using System.Globalization;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldTime;

namespace Octaryn.Server.Persistence.WorldSave;

internal static class ServerWorldSaveMetadataBuilder
{
    public static ServerWorldSaveMetadata Build(string worldRoot)
    {
        var worldTimePath = Path.Combine(worldRoot, "world_time.json");
        var hasWorldTime = WorldTimeStore.TryLoad(worldTimePath, out _);
        var playerCount = CountPlayers(worldRoot);
        var blockOverrideCount = CountBlockOverrides(Path.Combine(worldRoot, "world_blocks.json"));
        return new ServerWorldSaveMetadata(
            hasWorldTime || playerCount > 0 || blockOverrideCount > 0,
            hasWorldTime,
            playerCount > 0,
            blockOverrideCount > 0,
            playerCount,
            blockOverrideCount);
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
                !ServerPlayerSaveFile.TryLoad(path, out _))
            {
                continue;
            }

            playerIds.Add(playerId);
        }

        return playerIds.Count;
    }

    private static int CountBlockOverrides(string path)
    {
        return WorldBlockOverrideFile.TryLoad(path, out var file)
            ? file.Blocks.Count
            : 0;
    }
}
