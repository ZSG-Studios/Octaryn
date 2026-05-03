using System.Text.Json;
using System.Text.Json.Serialization;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class ChunkColumnOverrideFile
{
    private const int CurrentVersion = 2;

    private static readonly JsonSerializerOptions s_options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public int Version { get; init; } = CurrentVersion;

    public int Cx { get; init; }

    public int Cz { get; init; }

    public IReadOnlyList<ChunkColumnBlockOverrideRecord> Blocks { get; init; } = [];

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static ChunkColumnOverrideFile FromEdits(int originX, int originZ, IEnumerable<BlockEdit> edits)
    {
        var records = edits
            .OrderBy(edit => edit.Position.Y)
            .ThenBy(edit => edit.Position.X)
            .ThenBy(edit => edit.Position.Z)
            .Select(edit => new ChunkColumnBlockOverrideRecord(
                edit.Position.X,
                edit.Position.Y,
                edit.Position.Z,
                edit.Block.Value))
            .ToArray();

        return new ChunkColumnOverrideFile
        {
            Cx = originX,
            Cz = originZ,
            Blocks = records
        };
    }

    public IEnumerable<BlockEdit> ToEdits()
    {
        foreach (var block in Blocks)
        {
            yield return new BlockEdit(
                new BlockPosition(block.Bx, block.By, block.Bz),
                new BlockId(block.Block));
        }
    }

    public static bool TryLoad(string path, out ChunkColumnOverrideFile file)
    {
        file = new ChunkColumnOverrideFile();
        if (!File.Exists(path))
        {
            return false;
        }

        var loaded = JsonSerializer.Deserialize<ChunkColumnOverrideFile>(File.ReadAllText(path), s_options);
        if (loaded is null || !loaded.IsCurrent)
        {
            return false;
        }

        file = loaded;
        return true;
    }

    public static void Save(string path, ChunkColumnOverrideFile file)
    {
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var tempPath = $"{path}.tmp";
        File.WriteAllText(tempPath, JsonSerializer.Serialize(file, s_options));
        File.Move(tempPath, path, overwrite: true);
    }
}
