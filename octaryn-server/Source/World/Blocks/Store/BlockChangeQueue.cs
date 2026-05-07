using Octaryn.Shared.Networking;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class BlockChangeQueue : IDisposable
{
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

    public NativeBlockChangeSnapshotDrainReport DrainSnapshotReport(ServerSnapshotHeader* snapshotHeader, ulong tickId)
    {
        var report = default(NativeBlockChangeSnapshotDrainReport);
        NativeBlockStoreLibrary.BlockChangeQueueDrainSnapshotReport(
            Handle,
            snapshotHeader,
            tickId,
            &report);
        return report;
    }

    public int DrainSnapshotReportAndLog(ServerSnapshotHeader* snapshotHeader, ulong tickId)
    {
        var report = DrainSnapshotReport(snapshotHeader, tickId);
        if (report.Result != 0)
        {
            Octaryn.Server.LiveDebugLog.Write($"server_live_snapshot_drain result={report.Result} tick={tickId} requested_capacity={report.RequestedCapacity} pending_before={report.PendingBefore}");
            return report.Result;
        }

        Octaryn.Server.LiveDebugLog.Write($"server_live_snapshot_drain result=0 tick={tickId} requested_capacity={report.RequestedCapacity} pending_before={report.PendingBefore} written={report.Written}");
        return 0;
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
