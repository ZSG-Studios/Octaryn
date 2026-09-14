using Octaryn.Shared.Networking;

namespace Octaryn.Client.WorldPresentation;

// Copies authoritative edits before the caller reuses its snapshot memory.
internal sealed class BlockUpdateQueue
{
    internal const int Capacity = 4096;
    private readonly ReplicationChange[] _changes = new ReplicationChange[Capacity];
    private readonly object _gate = new();
    private int _head;
    private int _count;

    internal unsafe int Apply(in ServerSnapshotHeader snapshot)
    {
        if (snapshot.ReplicationCount != 0 ||
            (snapshot.ChangeCount > 0 && snapshot.ChangesAddress == 0))
        {
            return -2;
        }

        lock (_gate)
        {
            // Edits must not be dropped or partially accepted under backpressure.
            if (snapshot.ChangeCount > Capacity - _count)
            {
                return -3;
            }
            var source = new ReadOnlySpan<ReplicationChange>(
                (void*)snapshot.ChangesAddress, (int)snapshot.ChangeCount);
            foreach (ref readonly var change in source)
            {
                if (!BlockReplicationChange.TryRead(in change, out _))
                {
                    return -2;
                }
            }
            foreach (ref readonly var change in source)
            {
                _changes[(_head + _count++) % Capacity] = change;
            }
            return 0;
        }
    }

    internal uint Drain(Span<ReplicationChange> output)
    {
        lock (_gate)
        {
            int written = Math.Min(output.Length, _count);
            for (int index = 0; index < written; index++)
            {
                output[index] = _changes[_head];
                _changes[_head] = default;
                _head = (_head + 1) % Capacity;
            }
            _count -= written;
            return (uint)written;
        }
    }

    internal void Reset()
    {
        lock (_gate)
        {
            Array.Clear(_changes);
            _head = _count = 0;
        }
    }
}
