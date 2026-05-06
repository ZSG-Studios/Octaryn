using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class WorldBlockPersistence : IDisposable
{
    private const uint LoadSourceAggregateFile = 1;
    private const uint LoadSourceChunkDirectory = 2;

    private readonly string _path;
    private IntPtr _saveTracker;

    public WorldBlockPersistence(string path)
    {
        _path = path;
        _saveTracker = NativeWorldPersistenceLibrary.CreateWorldBlockSaveTracker();
    }

    ~WorldBlockPersistence()
    {
        Dispose();
    }

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

    public string Path => _path;

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
        var chunkColumnDirectory = ChunkDirectoryForAggregatePath(_path);
        var source = NativeWorldPersistenceLibrary.SelectWorldBlockLoadSource(chunkColumnDirectory, _path);
        if (source.Source == LoadSourceChunkDirectory)
        {
            return NativeWorldPersistenceLibrary.ReadChunkOverrideDirectory(chunkColumnDirectory);
        }

        return source.Source == LoadSourceAggregateFile &&
            NativeWorldPersistenceLibrary.TryReadWorldBlockOverrideFile(_path, out _, out var edits)
                ? edits
                : [];
    }

    public void EnsureInitialized(BlockStore blocks)
    {
        var snapshot = blocks.Snapshot();
        NativeWorldPersistenceLibrary.InitializeWorldBlockOverrides(
            _path,
            ChunkDirectoryForAggregatePath(_path),
            ToNativeEdits(snapshot));
    }

    public void MarkDirty()
    {
        NativeWorldPersistenceLibrary.MarkWorldBlockSaveTrackerDirty(Tracker);
    }

    public void SaveIfDirty(BlockStore blocks)
    {
        if (!NativeWorldPersistenceLibrary.ShouldSaveWorldBlockOverrides(Tracker))
        {
            return;
        }

        var snapshot = blocks.Snapshot();
        NativeWorldPersistenceLibrary.SaveWorldBlockOverrides(
            _path,
            ChunkDirectoryForAggregatePath(_path),
            ToNativeEdits(snapshot));
        NativeWorldPersistenceLibrary.MarkWorldBlockSaveTrackerClean(Tracker);
    }

    public void Dispose()
    {
        var tracker = _saveTracker;
        if (tracker == IntPtr.Zero)
        {
            return;
        }

        _saveTracker = IntPtr.Zero;
        NativeWorldPersistenceLibrary.DestroyWorldBlockSaveTracker(tracker);
        GC.SuppressFinalize(this);
    }

    private static NativePersistenceBlockEdit[] ToNativeEdits(IReadOnlyList<BlockEdit> edits)
    {
        return edits.Select(NativePersistenceBlockEdit.FromBlockEdit).ToArray();
    }

    private static string ChunkDirectoryForAggregatePath(string aggregatePath)
    {
        return System.IO.Path.GetDirectoryName(aggregatePath) ?? ".";
    }

    private IntPtr Tracker
    {
        get
        {
            ObjectDisposedException.ThrowIf(_saveTracker == IntPtr.Zero, this);
            return _saveTracker;
        }
    }
}
