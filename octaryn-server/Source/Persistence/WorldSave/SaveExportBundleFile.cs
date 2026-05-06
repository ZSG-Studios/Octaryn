using System.Text.Json;
using System.Text.Json.Serialization;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldTime;
using Octaryn.Server.World.Time;

namespace Octaryn.Server.Persistence.WorldSave;

internal sealed class SaveExportBundleFile
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

    public static SaveExportBundleFile FromWorldRoot(string worldRoot)
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

        return new SaveExportBundleFile
        {
            WorldTime = worldTime,
            Players = LoadPlayers(worldRoot),
            Chunks = LoadChunks(worldRoot)
        };
    }

    public static bool TryLoadGzip(string path, out SaveExportBundleFile bundle)
    {
        bundle = new SaveExportBundleFile();
        if (!File.Exists(path))
        {
            return false;
        }

        if (!NativeWorldPersistenceLibrary.TryReadGzipFile(path, out var payload))
        {
            return false;
        }

        SaveExportBundleFile? loaded;
        try
        {
            loaded = JsonSerializer.Deserialize<SaveExportBundleFile>(payload, s_options);
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

    public static void SaveGzip(string path, SaveExportBundleFile bundle)
    {
        var payload = JsonSerializer.SerializeToUtf8Bytes(bundle, s_options);
        NativeWorldPersistenceLibrary.WriteGzipFile(path, payload);
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

            NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(
                worldRoot,
                player.Id,
                player.Data.ToNativeState());
        }

        var chunkFiles = Chunks.Select(chunk =>
        {
            if (!ChunkColumnOverrideFile.TryNormalize(chunk, out var normalized))
            {
                throw new InvalidOperationException("Unsupported chunk override version.");
            }

            return normalized;
        }).ToArray();

        var edits = chunkFiles
            .SelectMany(chunk => chunk.ToEdits())
            .Select(NativePersistenceBlockEdit.FromBlockEdit)
            .ToArray();
        NativeWorldPersistenceLibrary.SaveWorldBlockOverrides(
            Path.Combine(worldRoot, "world_blocks.json"),
            worldRoot,
            edits);
    }

    private static IReadOnlyList<PlayerExportEntry> LoadPlayers(string worldRoot)
    {
        return NativeWorldPersistenceLibrary.ReadPlayerDirectory(worldRoot)
            .Select(player => new PlayerExportEntry(player.PlayerId, PlayerSaveFile.FromNativeState(player.State)))
            .ToArray();
    }

    private static IReadOnlyList<ChunkColumnOverrideFile> LoadChunks(string worldRoot)
    {
        using var persistence = new WorldBlockPersistence(Path.Combine(worldRoot, "world_blocks.json"));
        var plan = NativeWorldPersistenceLibrary.PlanChunkColumns(persistence.ReadNativeEdits());
        return plan.Columns
            .Select(column => ChunkColumnOverrideFile.FromNativeEdits(
                column.OriginX,
                column.OriginZ,
                plan.NativeEditsFor(column)))
            .ToArray();
    }
}

internal sealed record PlayerExportEntry(int Id, PlayerSaveFile Data);
