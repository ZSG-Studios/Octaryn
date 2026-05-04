using System.Text.Json;
using System.Text.Json.Serialization;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class WorldBlockOverrideFile
{
    private const int CurrentVersion = 1;

    private static readonly JsonSerializerOptions s_options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public int Version { get; init; } = CurrentVersion;

    public IReadOnlyList<WorldBlockOverrideRecord> Blocks { get; init; } = [];

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static WorldBlockOverrideFile FromEdits(IEnumerable<BlockEdit> edits)
    {
        var records = edits
            .OrderBy(edit => edit.Position.X)
            .ThenBy(edit => edit.Position.Y)
            .ThenBy(edit => edit.Position.Z)
            .Select(edit => new WorldBlockOverrideRecord(
                edit.Position.X,
                edit.Position.Y,
                edit.Position.Z,
                edit.Block.Value))
            .ToArray();

        return new WorldBlockOverrideFile { Blocks = records };
    }

    public IEnumerable<BlockEdit> ToEdits()
    {
        foreach (var block in Blocks)
        {
            yield return new BlockEdit(
                new BlockPosition(block.X, block.Y, block.Z),
                new BlockId(block.Block));
        }
    }

    public static bool TryLoad(string path, out WorldBlockOverrideFile file)
    {
        file = new WorldBlockOverrideFile();
        if (!File.Exists(path))
        {
            return false;
        }

        var loaded = JsonSerializer.Deserialize<WorldBlockOverrideFile>(File.ReadAllText(path), s_options);
        if (loaded is null || !loaded.IsCurrent)
        {
            return false;
        }

        file = loaded;
        return true;
    }

    public static void Save(string path, WorldBlockOverrideFile file)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var tempPath = $"{path}.tmp";
        File.WriteAllText(tempPath, JsonSerializer.Serialize(file, s_options));
        File.Move(tempPath, path, overwrite: true);
    }
}
