using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Shared.World;

internal static class ChunkColumnOverrideProbeFile
{
    public static bool TryLoad(string path, out ChunkColumnOverrideFile file)
    {
        file = new ChunkColumnOverrideFile();
        if (!NativeWorldPersistenceLibrary.TryReadChunkOverrideFile(path, out var nativeFile, out var blocks))
        {
            return false;
        }

        file = new ChunkColumnOverrideFile
        {
            Version = checked((int)nativeFile.Version),
            Cx = nativeFile.Cx,
            Cz = nativeFile.Cz,
            Blocks = blocks.Select(block => block.ToBlock()).ToArray()
        };
        return true;
    }

    public static bool TryNormalize(ChunkColumnOverrideFile loaded, out ChunkColumnOverrideFile file)
    {
        var blocks = loaded.Blocks
            .Select(NativePersistenceChunkOverrideBlock.FromBlock)
            .ToArray();
        if (!NativeWorldPersistenceLibrary.TryNormalizeChunkOverrideFile(
            new NativePersistenceChunkOverrideFile(
                checked((uint)loaded.Version),
                loaded.Cx,
                loaded.Cz,
                checked((uint)blocks.Length)),
            blocks,
            out var normalizedFile,
            out var normalizedBlocks))
        {
            file = loaded;
            return false;
        }

        file = new ChunkColumnOverrideFile
        {
            Version = checked((int)normalizedFile.Version),
            Cx = normalizedFile.Cx,
            Cz = normalizedFile.Cz,
            Blocks = normalizedBlocks.Select(block => block.ToBlock()).ToArray()
        };
        return true;
    }

    public static void Save(string path, ChunkColumnOverrideFile file)
    {
        var blocks = file.Blocks
            .Select(NativePersistenceChunkOverrideBlock.FromBlock)
            .ToArray();
        NativeWorldPersistenceLibrary.WriteChunkOverrideFile(
            path,
            new NativePersistenceChunkOverrideFile(
                checked((uint)file.Version),
                file.Cx,
                file.Cz,
                checked((uint)blocks.Length)),
            blocks);
    }

    public static IEnumerable<BlockEdit> ToEdits(ChunkColumnOverrideFile file)
    {
        foreach (var block in file.Blocks)
        {
            yield return new BlockEdit(
                new BlockPosition(block.Bx, block.By, block.Bz),
                new BlockId(block.Block));
        }
    }
}
