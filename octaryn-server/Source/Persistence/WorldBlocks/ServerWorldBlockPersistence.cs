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

        WorldBlockOverrideFile.Save(path, WorldBlockOverrideFile.FromEdits(blocks.Snapshot()));
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

        WorldBlockOverrideFile.Save(path, WorldBlockOverrideFile.FromEdits(blocks.Snapshot()));
        _dirty = false;
    }
}
