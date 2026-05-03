using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class ServerWorldBlockPersistence(string path)
{
    private bool _dirty;

    public static ServerWorldBlockPersistence FromEnvironment()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return new ServerWorldBlockPersistence(explicitPath);
        }

        var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        if (string.IsNullOrWhiteSpace(presetName))
        {
            presetName = "debug-linux";
        }

        return new ServerWorldBlockPersistence(System.IO.Path.Combine(
            "build",
            presetName,
            "server",
            "world",
            "world_blocks.json"));
    }

    public string Path => path;

    public void Load(ServerBlockStore blocks)
    {
        var chunkColumnDirectory = ChunkColumnOverrideStore.DirectoryForWorldBlocksPath(path);
        if (ChunkColumnOverrideStore.HasCurrentFilesAtLeastAsNewAs(chunkColumnDirectory, path))
        {
            var chunkColumnEdits = ChunkColumnOverrideStore.LoadEdits(chunkColumnDirectory);
            if (chunkColumnEdits.Count > 0)
            {
                blocks.Load(chunkColumnEdits);
                return;
            }
        }

        if (WorldBlockOverrideFile.TryLoad(path, out var file))
        {
            blocks.Load(file.ToEdits());
        }
    }

    public void EnsureInitialized(ServerBlockStore blocks)
    {
        if (File.Exists(path))
        {
            return;
        }

        var snapshot = blocks.Snapshot();
        WorldBlockOverrideFile.Save(path, WorldBlockOverrideFile.FromEdits(snapshot));
        ChunkColumnOverrideStore.SaveEdits(ChunkColumnOverrideStore.DirectoryForWorldBlocksPath(path), snapshot);
    }

    public void MarkDirty()
    {
        _dirty = true;
    }

    public void SaveIfDirty(ServerBlockStore blocks)
    {
        if (!_dirty)
        {
            return;
        }

        var snapshot = blocks.Snapshot();
        WorldBlockOverrideFile.Save(path, WorldBlockOverrideFile.FromEdits(snapshot));
        ChunkColumnOverrideStore.SaveEdits(ChunkColumnOverrideStore.DirectoryForWorldBlocksPath(path), snapshot);
        _dirty = false;
    }
}
