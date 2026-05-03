using System.Globalization;
using System.IO.Compression;
using System.Text.Json;
using System.Text.Json.Serialization;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldTime;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldSave;

internal sealed class ServerSaveExportBundleFile
{
    private const int CurrentVersion = 1;

    private static readonly JsonSerializerOptions s_options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public int Version { get; init; } = CurrentVersion;

    public WorldTimeFile? WorldTime { get; init; }

    public IReadOnlyList<PlayerExportEntry> Players { get; init; } = [];

    public IReadOnlyList<ChunkColumnOverrideFile> Chunks { get; init; } = [];

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static ServerSaveExportBundleFile FromWorldRoot(string worldRoot)
    {
        WorldTimeFile? worldTime = null;
        if (WorldTimeStore.TryLoad(Path.Combine(worldRoot, "world_time.json"), out var worldTimeBlob))
        {
            worldTime = new WorldTimeFile
            {
                Version = worldTimeBlob.Version,
                DayIndex = worldTimeBlob.DayIndex,
                SecondsOfDay = worldTimeBlob.SecondsOfDay
            };
        }

        return new ServerSaveExportBundleFile
        {
            WorldTime = worldTime,
            Players = LoadPlayers(worldRoot),
            Chunks = LoadChunks(worldRoot)
        };
    }

    public static bool TryLoadGzip(string path, out ServerSaveExportBundleFile bundle)
    {
        bundle = new ServerSaveExportBundleFile();
        if (!File.Exists(path))
        {
            return false;
        }

        ServerSaveExportBundleFile? loaded;
        try
        {
            using var file = File.OpenRead(path);
            using var gzip = new GZipStream(file, CompressionMode.Decompress);
            loaded = JsonSerializer.Deserialize<ServerSaveExportBundleFile>(gzip, s_options);
        }
        catch (IOException)
        {
            return false;
        }
        catch (InvalidDataException)
        {
            return false;
        }
        catch (JsonException)
        {
            return false;
        }

        if (loaded is null || !loaded.IsCurrent)
        {
            return false;
        }

        bundle = loaded;
        return true;
    }

    public static void SaveGzip(string path, ServerSaveExportBundleFile bundle)
    {
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var tempPath = $"{path}.tmp";
        using (var file = File.Create(tempPath))
        using (var gzip = new GZipStream(file, CompressionLevel.SmallestSize))
        {
            JsonSerializer.Serialize(gzip, bundle, s_options);
        }

        File.Move(tempPath, path, overwrite: true);
    }

    public void WriteToWorldRoot(string worldRoot)
    {
        if (!IsCurrent)
        {
            throw new InvalidOperationException("Unsupported save export bundle version.");
        }

        if (WorldTime is not null)
        {
            if (WorldTime.Version != WorldTimeBlob.CurrentVersion)
            {
                throw new InvalidOperationException("Unsupported world time version.");
            }

            WorldTimeStore.Save(
                Path.Combine(worldRoot, "world_time.json"),
                new WorldTimeBlob(WorldTime.Version, WorldTime.DayIndex, WorldTime.SecondsOfDay));
        }

        foreach (var player in Players)
        {
            if (!player.Data.IsCurrent)
            {
                throw new InvalidOperationException("Unsupported player save version.");
            }

            ServerPlayerSaveFile.Save(
                Path.Combine(worldRoot, $"player_{player.Id.ToString(CultureInfo.InvariantCulture)}.json"),
                player.Data.ToState());
        }

        var chunkFiles = Chunks.Select(chunk =>
        {
            if (!ChunkColumnOverrideFile.TryNormalize(chunk, out var normalized))
            {
                throw new InvalidOperationException("Unsupported chunk override version.");
            }

            return normalized;
        }).ToArray();

        var edits = chunkFiles.SelectMany(chunk => chunk.ToEdits()).ToArray();
        WorldBlockOverrideFile.Save(
            Path.Combine(worldRoot, "world_blocks.json"),
            WorldBlockOverrideFile.FromEdits(edits));

        ChunkColumnOverrideStore.SaveEdits(worldRoot, edits);
    }

    private static IReadOnlyList<PlayerExportEntry> LoadPlayers(string worldRoot)
    {
        if (!Directory.Exists(worldRoot))
        {
            return [];
        }

        List<PlayerExportEntry> players = [];
        foreach (var path in Directory.EnumerateFiles(worldRoot, "player_*.json"))
        {
            var name = Path.GetFileNameWithoutExtension(path);
            if (name.Length <= "player_".Length ||
                !int.TryParse(name["player_".Length..], NumberStyles.Integer, CultureInfo.InvariantCulture, out var playerId) ||
                !ServerPlayerSaveFile.TryLoad(path, out var state))
            {
                continue;
            }

            players.Add(new PlayerExportEntry(playerId, ServerPlayerSaveFile.FromState(state)));
        }

        return players.OrderBy(player => player.Id).ToArray();
    }

    private static IReadOnlyList<ChunkColumnOverrideFile> LoadChunks(string worldRoot)
    {
        var blocks = new ServerBlockStore();
        var persistence = new ServerWorldBlockPersistence(Path.Combine(worldRoot, "world_blocks.json"));
        persistence.Load(blocks);
        var edits = blocks.Snapshot();

        return edits
            .GroupBy(edit => ChunkColumnOriginFor(edit.Position))
            .OrderBy(group => group.Key.X)
            .ThenBy(group => group.Key.Z)
            .Select(group => ChunkColumnOverrideFile.FromEdits(group.Key.X, group.Key.Z, group))
            .ToArray();
    }

    private static ChunkColumnOrigin ChunkColumnOriginFor(BlockPosition position)
    {
        return new ChunkColumnOrigin(
            FloorDiv(position.X, ServerBlockLimits.ChunkWidth) * ServerBlockLimits.ChunkWidth,
            FloorDiv(position.Z, ServerBlockLimits.ChunkDepth) * ServerBlockLimits.ChunkDepth);
    }

    private static int FloorDiv(int value, int divisor)
    {
        var quotient = value / divisor;
        var remainder = value % divisor;
        return remainder < 0 ? quotient - 1 : quotient;
    }

    private readonly record struct ChunkColumnOrigin(int X, int Z);
}

internal sealed record PlayerExportEntry(int Id, ServerPlayerSaveFile Data);
