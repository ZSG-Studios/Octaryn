namespace Octaryn.Server.Persistence.World;

// One ordered writer per player. Acceptance is not persistence acknowledgement.
internal sealed class PlayerSaveQueue : IDisposable
{
    private readonly OrderedSaveQueue<NativePersistencePlayerState> _queue;

    public PlayerSaveQueue(string directory, int playerId)
        : this(state => NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(directory, playerId, state)) { }

    internal PlayerSaveQueue(Action<NativePersistencePlayerState> write)
    {
        _queue = new(write, "octaryn-player-save");
    }

    public bool HasCapacity => _queue.HasCapacity;
    public OrderedSaveQueue<NativePersistencePlayerState>.Receipt? Completed => _queue.Completed;
    public bool TryEnqueue(NativePersistencePlayerState state) => _queue.TryEnqueue(state);
    public void Flush() => _queue.Flush();
    public void Dispose() => _queue.Dispose();
}
