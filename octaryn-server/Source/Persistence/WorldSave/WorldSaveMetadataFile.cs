using System.Text.Json;
using System.Text.Json.Serialization;

namespace Octaryn.Server.Persistence.WorldSave;

internal sealed class WorldSaveMetadataFile
{
    private const int CurrentVersion = 1;

    private static readonly JsonSerializerOptions s_options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public int Version { get; init; } = CurrentVersion;

    public bool SaveExists { get; init; }

    public bool HasWorldTime { get; init; }

    public bool HasPlayerData { get; init; }

    public bool HasWorldData { get; init; }

    public int PlayerCount { get; init; }

    public int ChunkOverrideCount { get; init; }

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static WorldSaveMetadataFile FromMetadata(WorldSaveMetadata metadata)
    {
        return new WorldSaveMetadataFile
        {
            SaveExists = metadata.SaveExists,
            HasWorldTime = metadata.HasWorldTime,
            HasPlayerData = metadata.HasPlayerData,
            HasWorldData = metadata.HasWorldData,
            PlayerCount = metadata.PlayerCount,
            ChunkOverrideCount = metadata.ChunkOverrideCount
        };
    }

    public WorldSaveMetadata ToMetadata()
    {
        return new WorldSaveMetadata(
            SaveExists,
            HasWorldTime,
            HasPlayerData,
            HasWorldData,
            PlayerCount,
            ChunkOverrideCount);
    }

    public static bool TryLoad(string path, out WorldSaveMetadata metadata)
    {
        metadata = default;
        if (!File.Exists(path))
        {
            return false;
        }

        var file = JsonSerializer.Deserialize<WorldSaveMetadataFile>(File.ReadAllText(path), s_options);
        if (file is null || !file.IsCurrent)
        {
            return false;
        }

        metadata = file.ToMetadata();
        return true;
    }

    public static void Save(string path, WorldSaveMetadata metadata)
    {
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var tempPath = $"{path}.tmp";
        File.WriteAllText(tempPath, JsonSerializer.Serialize(FromMetadata(metadata), s_options));
        File.Move(tempPath, path, overwrite: true);
    }
}
