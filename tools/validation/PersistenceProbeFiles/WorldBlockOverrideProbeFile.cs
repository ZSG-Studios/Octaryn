using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Shared.World;

internal sealed class WorldBlockOverrideProbeFile
{
    private const int CurrentVersion = 1;

    public int Version { get; init; } = CurrentVersion;

    public IReadOnlyList<WorldBlockOverrideProbeRecord> Blocks { get; init; } = [];

    public static WorldBlockOverrideProbeFile FromEdits(IEnumerable<BlockEdit> edits)
    {
        var records = edits
            .OrderBy(edit => edit.Position.X)
            .ThenBy(edit => edit.Position.Y)
            .ThenBy(edit => edit.Position.Z)
            .Select(edit => new WorldBlockOverrideProbeRecord(
                edit.Position.X,
                edit.Position.Y,
                edit.Position.Z,
                edit.Block.Value))
            .ToArray();

        return new WorldBlockOverrideProbeFile { Blocks = records };
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

    public static bool TryLoad(string path, out WorldBlockOverrideProbeFile file)
    {
        file = new WorldBlockOverrideProbeFile();
        if (!NativeWorldPersistenceLibrary.TryReadWorldBlockOverrideFile(path, out var nativeFile, out var blocks))
        {
            return false;
        }

        file = new WorldBlockOverrideProbeFile
        {
            Version = checked((int)nativeFile.Version),
            Blocks = blocks.Select(block => new WorldBlockOverrideProbeRecord(
                block.Position.X,
                block.Position.Y,
                block.Position.Z,
                block.Block)).ToArray()
        };
        return true;
    }

    public static void Save(string path, WorldBlockOverrideProbeFile file)
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

internal sealed record WorldBlockOverrideProbeRecord(int X, int Y, int Z, ushort Block);
