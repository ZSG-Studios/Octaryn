using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class BlockChangeQueue : IDisposable
{
    public const uint BlockEditChangeKind = BlockReplicationChange.ChangeKind;

    private IntPtr _handle;

    public BlockChangeQueue()
    {
        _handle = NativeBlockStoreLibrary.BlockChangeQueueCreate();
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native server block change queue allocation failed.");
        }
    }

    ~BlockChangeQueue()
    {
        Dispose();
    }

    public int PendingCount => checked((int)NativeBlockStoreLibrary.BlockChangeQueuePendingCount(Handle));

    internal IntPtr NativeHandle => Handle;

    public void Enqueue(BlockEdit edit)
    {
        var nativeEdit = NativeBlockEdit.FromBlockEdit(edit);
        NativeBlockStoreLibrary.BlockChangeQueueEnqueue(Handle, &nativeEdit);
    }

    public int Drain(ReplicationChange* changes, uint capacity, ulong tickId, out uint written)
    {
        uint nativeWritten = 0;
        var result = NativeBlockStoreLibrary.BlockChangeQueueDrain(Handle, changes, capacity, tickId, &nativeWritten);
        written = nativeWritten;
        return result;
    }

    public int DrainSnapshot(ServerSnapshotHeader* snapshotHeader, ulong tickId, out ulong pendingBefore, out uint written)
    {
        ulong nativePendingBefore = 0;
        uint nativeWritten = 0;
        var result = NativeBlockStoreLibrary.BlockChangeQueueDrainSnapshot(
            Handle,
            snapshotHeader,
            tickId,
            &nativePendingBefore,
            &nativeWritten);
        pendingBefore = nativePendingBefore;
        written = nativeWritten;
        return result;
    }

    public void Dispose()
    {
        var handle = _handle;
        if (handle == IntPtr.Zero)
        {
            return;
        }

        _handle = IntPtr.Zero;
        NativeBlockStoreLibrary.BlockChangeQueueDestroy(handle);
        GC.SuppressFinalize(this);
    }

    private IntPtr Handle
    {
        get
        {
            ObjectDisposedException.ThrowIf(_handle == IntPtr.Zero, this);
            return _handle;
        }
    }
}
