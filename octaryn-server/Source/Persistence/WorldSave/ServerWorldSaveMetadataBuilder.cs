using System.Globalization;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldTime;
using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.Persistence.WorldSave;

internal static class ServerWorldSaveMetadataBuilder
{
    public static ServerWorldSaveMetadata Build(string worldRoot)
    {
        var worldTimePath = Path.Combine(worldRoot, "world_time.json");
        var hasWorldTime = WorldTimeStore.TryLoad(worldTimePath, out _);
        var playerCount = CountPlayers(worldRoot);
        var chunkOverrideCount = CountChunkOverrides(worldRoot);
        return new ServerWorldSaveMetadata(
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
                !ServerPlayerSaveFile.TryLoad(path, out _))
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
            ? file.ToEdits()
                .Select(edit => (
                    X: FloorDiv(edit.Position.X, ServerBlockLimits.ChunkWidth),
                    Z: FloorDiv(edit.Position.Z, ServerBlockLimits.ChunkDepth)))
                .Distinct()
                .Count()
            : 0;
    }

    private static int FloorDiv(int value, int divisor)
    {
        var quotient = value / divisor;
        var remainder = value % divisor;
        return remainder < 0 ? quotient - 1 : quotient;
    }
}
