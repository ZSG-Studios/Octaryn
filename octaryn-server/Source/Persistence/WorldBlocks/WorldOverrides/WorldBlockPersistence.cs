using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class WorldBlockPersistence(string path)
{
    private const uint LoadSourceAggregateFile = 1;
    private const uint LoadSourceChunkDirectory = 2;

    private bool _dirty;

    public static WorldBlockPersistence FromEnvironment()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return new WorldBlockPersistence(explicitPath);
        }

        var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        if (string.IsNullOrWhiteSpace(presetName))
        {
            presetName = "debug-linux";
        }

        return new WorldBlockPersistence(System.IO.Path.Combine(
            "build",
            presetName,
            "server",
            "world",
            "world_blocks.json"));
    }

    public string Path => path;

    public void Load(BlockStore blocks)
    {
        var edits = ReadNativeEdits();
        if (edits.Length > 0)
        {
            blocks.Load(edits.Select(edit => edit.ToBlockEdit()));
        }
    }

    internal NativePersistenceBlockEdit[] ReadNativeEdits()
    {
        var chunkColumnDirectory = ChunkColumnOverrideStore.DirectoryForWorldBlocksPath(path);
        var source = NativeWorldPersistenceLibrary.SelectWorldBlockLoadSource(chunkColumnDirectory, path);
        if (source.Source == LoadSourceChunkDirectory)
        {
            return NativeWorldPersistenceLibrary.ReadChunkOverrideDirectory(chunkColumnDirectory);
        }

        return source.Source == LoadSourceAggregateFile &&
            NativeWorldPersistenceLibrary.TryReadWorldBlockOverrideFile(path, out _, out var edits)
                ? edits
                : [];
    }

    public void EnsureInitialized(BlockStore blocks)
    {
        var snapshot = blocks.Snapshot();
        NativeWorldPersistenceLibrary.InitializeWorldBlockOverrides(
            path,
            ChunkColumnOverrideStore.DirectoryForWorldBlocksPath(path),
            ToNativeEdits(snapshot));
    }

    public void MarkDirty()
    {
        _dirty = true;
    }

    public void SaveIfDirty(BlockStore blocks)
    {
        if (!_dirty)
        {
            return;
        }

        var snapshot = blocks.Snapshot();
        NativeWorldPersistenceLibrary.SaveWorldBlockOverrides(
            path,
            ChunkColumnOverrideStore.DirectoryForWorldBlocksPath(path),
            ToNativeEdits(snapshot));
        _dirty = false;
    }

    private static NativePersistenceBlockEdit[] ToNativeEdits(IReadOnlyList<BlockEdit> edits)
    {
        return edits.Select(NativePersistenceBlockEdit.FromBlockEdit).ToArray();
    }
}
