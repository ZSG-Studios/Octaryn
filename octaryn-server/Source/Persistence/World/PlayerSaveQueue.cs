using System.Runtime.ExceptionServices;
using System.Threading.Channels;

namespace Octaryn.Server.Persistence.World;

// One ordered writer per player. Acceptance is not persistence acknowledgement.
internal sealed class PlayerSaveQueue : IDisposable
{
    internal sealed record Receipt(ulong Sequence, NativePersistencePlayerState State);
    private readonly Channel<Receipt> _queue = Channel.CreateBounded<Receipt>(new BoundedChannelOptions(2)
    { SingleReader = true, SingleWriter = true, FullMode = BoundedChannelFullMode.Wait });
    private readonly Action<NativePersistencePlayerState> _write;
    private readonly Thread _worker;
    private Receipt? _completed;
    private ExceptionDispatchInfo? _failure;
    private ulong _sequence;
    private int _pending;
    private bool _disposed;

    public PlayerSaveQueue(string directory, int playerId)
        : this(state => NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(directory, playerId, state)) { }

    internal PlayerSaveQueue(Action<NativePersistencePlayerState> write)
    {
        _write = write;
        _worker = new Thread(Run) { IsBackground = true, Name = "octaryn-player-save" };
        _worker.Start();
    }

    public bool HasCapacity { get { CheckFailure(); return Volatile.Read(ref _pending) < 2; } }
    public Receipt? Completed { get { CheckFailure(); return Volatile.Read(ref _completed); } }

    public bool TryEnqueue(NativePersistencePlayerState state)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!HasCapacity) return false;
        var receipt = new Receipt(_sequence + 1, state);
        Interlocked.Increment(ref _pending);
        if (!_queue.Writer.TryWrite(receipt)) { Interlocked.Decrement(ref _pending); return false; }
        _sequence = receipt.Sequence;
        return true;
    }

    private void Run()
    {
        try
        {
            while (_queue.Reader.WaitToReadAsync().AsTask().GetAwaiter().GetResult())
                while (_queue.Reader.TryRead(out var receipt))
                {
                    _write(receipt.State);
                    Volatile.Write(ref _completed, receipt);
                    Interlocked.Decrement(ref _pending);
                    lock (_queue) Monitor.PulseAll(_queue);
                }
        }
        catch (Exception error)
        {
            Volatile.Write(ref _failure, ExceptionDispatchInfo.Capture(error));
            lock (_queue) Monitor.PulseAll(_queue);
        }
    }

    public void Flush()
    {
        lock (_queue)
        {
            while (Volatile.Read(ref _pending) != 0)
            {
                CheckFailure();
                Monitor.Wait(_queue);
            }
        }
        CheckFailure();
    }

    private void CheckFailure() => Volatile.Read(ref _failure)?.Throw();

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _queue.Writer.TryComplete();
        _worker.Join();
        CheckFailure();
    }
}
