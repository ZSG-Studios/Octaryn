using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class BlockChangeQueue
{
    public const uint BlockEditChangeKind = BlockReplicationChange.ChangeKind;

    private readonly Queue<BlockEdit> _changes = new();

    public int PendingCount => _changes.Count;

    public void Enqueue(BlockEdit edit)
    {
        _changes.Enqueue(edit);
    }

    public unsafe int Drain(ReplicationChange* changes, uint capacity, ulong tickId, out uint written)
    {
        written = 0;
        if (_changes.Count == 0)
        {
            return 0;
        }

        if (changes is null || capacity < _changes.Count)
        {
            return -1;
        }

        while (_changes.TryDequeue(out var edit))
        {
            changes[written++] = ToReplicationChange(edit, tickId);
        }

        return 0;
    }

    private static ReplicationChange ToReplicationChange(BlockEdit edit, ulong tickId)
    {
        return new BlockReplicationChange(edit.Position, edit.Block).ToReplicationChange(tickId);
    }
}
