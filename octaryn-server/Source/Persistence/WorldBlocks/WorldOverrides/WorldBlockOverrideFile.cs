using System.Text.Json.Serialization;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class WorldBlockOverrideFile
{
    private const int CurrentVersion = 1;

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
        if (!NativeWorldPersistenceLibrary.TryReadWorldBlockOverrideFile(path, out var nativeFile, out var blocks))
        {
            return false;
        }

        file = new WorldBlockOverrideFile
        {
            Version = checked((int)nativeFile.Version),
            Blocks = blocks.Select(block => new WorldBlockOverrideRecord(
                block.Position.X,
                block.Position.Y,
                block.Position.Z,
                block.Block)).ToArray()
        };
        return true;
    }

    public static void Save(string path, WorldBlockOverrideFile file)
    {
        var blocks = file.Blocks
            .Select(block => NativePersistenceBlockEdit.FromBlockEdit(
                new BlockEdit(
                    new BlockPosition(block.X, block.Y, block.Z),
                    new BlockId(block.Block))))
            .ToArray();
        NativeWorldPersistenceLibrary.WriteWorldBlockOverrideFile(
            path,
            new NativePersistenceWorldBlockOverrideFile(
                checked((uint)file.Version),
                checked((uint)blocks.Length)),
            blocks);
    }
}
