using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class WorldBlockPersistence : IDisposable
{
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
        return new WorldBlockPersistence(NativeWorldPersistenceLibrary.WorldBlockOverridePathFromEnvironment());
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
        return NativeWorldPersistenceLibrary.ReadWorldBlockOverrides(
            _path,
            ChunkDirectoryForAggregatePath(_path));
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
        return NativeWorldPersistenceLibrary.ChunkDirectoryForAggregatePath(aggregatePath);
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
