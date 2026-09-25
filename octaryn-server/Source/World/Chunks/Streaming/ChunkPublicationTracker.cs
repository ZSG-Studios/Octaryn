namespace Octaryn.Server.World.Chunks;

// One process-stream publication state per authority instance. Changing the
// destination requires a fresh full baseline; other snapshot APIs do not ack it.
internal sealed class ChunkPublicationTracker
{
    private string? _lastPath;
    private NativeChunkViewIntent _lastWindow;
    private string? _fullPath;
    private ulong _fullRevision;

    public void Reset()
    {
        _lastPath = null;
        _lastWindow = default;
        _fullPath = null;
        _fullRevision = 0;
    }

    public bool NeedsFullSnapshot(string path, ulong revision) =>
        _fullPath != path || _fullRevision != revision;

    public bool ShouldPublish(string path, NativeChunkViewIntent window, ulong revision, bool submittedCommands) =>
        submittedCommands || NeedsFullSnapshot(path, revision) || _lastPath != path ||
        _lastWindow.Epoch != window.Epoch || _lastWindow.CenterChunkX != window.CenterChunkX ||
        _lastWindow.CenterChunkZ != window.CenterChunkZ || _lastWindow.Radius != window.Radius;

    public T Write<T>(string path, NativeChunkViewIntent window, ulong revision, bool metadataOnly, Func<T> writer)
    {
        var result = writer();
        _lastPath = path;
        _lastWindow = window;
        if (!metadataOnly)
        {
            _fullPath = path;
            _fullRevision = revision;
        }
        else if (_fullPath != path) _fullPath = null;
        return result;
    }
}
