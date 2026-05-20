using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class ClientBlockCommandQueue : IDisposable
{
    private readonly BlockEditService _blockEdits;
    private readonly BlockChangeQueue? _blockChanges;
    private readonly IBlockAuthorityRules _authorityRules;
    private readonly Action<int>? _changedEdits;
    private readonly Func<HostCommand, bool>? _canPlaceAgainstPlayer;
    private IntPtr _handle;

    public ClientBlockCommandQueue(
        BlockEditService blockEdits,
        IBlockAuthorityRules authorityRules,
        BlockChangeQueue? blockChanges = null,
        Action<int>? changedEdits = null,
        Func<HostCommand, bool>? canPlaceAgainstPlayer = null)
    {
        _blockEdits = blockEdits;
        _authorityRules = authorityRules;
        _blockChanges = blockChanges;
        _changedEdits = changedEdits;
        _canPlaceAgainstPlayer = canPlaceAgainstPlayer;
        _handle = NativeBlockStoreLibrary.ClientBlockCommandQueueCreate();
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native server client block command queue allocation failed.");
        }
    }

    ~ClientBlockCommandQueue()
    {
        Dispose();
    }

    public int PendingCount => checked((int)NativeBlockStoreLibrary.ClientBlockCommandQueuePendingCount(Handle));

    public static int MaxPendingCommands => checked((int)NativeBlockStoreLibrary.ClientBlockCommandQueueMaxPending());

    public NativeClientBlockCommandSubmitReport Submit(HostCommand* commands, uint commandCount)
    {
        var handle = GCHandle.Alloc(this);
        try
        {
            NativeClientBlockCommandSubmitReport report = default;
            _ = NativeBlockStoreLibrary.ClientBlockCommandQueueSubmitReport(
                Handle,
                commands,
                commandCount,
                &IsClientPlaceable,
                &CanApplyCommand,
                (void*)GCHandle.ToIntPtr(handle),
                &report);
            return report;
        }
        finally
        {
            handle.Free();
        }
    }

    public int SubmitAndLog(HostCommand* commands, uint commandCount)
    {
        var report = Submit(commands, commandCount);
        LogSubmitReport(commands, report);
        return report.Result;
    }

    public int Drain()
    {
        var report = _blockEdits.DrainClientCommandQueue(Handle, _blockChanges, ApplyQueuedCommandResult);
        Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_drain applied={report.Applied} pending={report.PendingAfter}");
        return report.Applied;
    }

    public void Dispose()
    {
        var handle = _handle;
        if (handle == IntPtr.Zero)
        {
            return;
        }

        _handle = IntPtr.Zero;
        NativeBlockStoreLibrary.ClientBlockCommandQueueDestroy(handle);
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

    private bool ApplyQueuedCommandResult(HostCommand command, BlockEditResult result)
    {
        BlockCommandLiveLog.WriteResult(command, result);
        if (result.Changed)
        {
            _changedEdits?.Invoke(result.Changes.Count);
        }

        return result.Applied;
    }

    private static void LogSubmitReport(HostCommand* commands, NativeClientBlockCommandSubmitReport report)
    {
        Octaryn.Server.LiveDebugLog.Write($"server_live_client_commands_submit requested={report.RequestedCount} pending_before={report.PendingBefore}");
        if (report.Reason == NativeClientBlockCommandSubmitReason.Capacity)
        {
            Octaryn.Server.LiveDebugLog.Write($"server_live_client_commands_submit result={report.Result} reason={NativeBlockStoreLibrary.ClientBlockCommandSubmitReasonLabel(report.Reason)} requested={report.RequestedCount}");
            return;
        }

        if (report.Reason == NativeClientBlockCommandSubmitReason.RejectedCommand)
        {
            var rejectedCommand = commands[checked((int)report.RejectedIndex)];
            Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_rejected index={report.RejectedIndex} kind={rejectedCommand.Kind} request={rejectedCommand.RequestId} edit={NativeBlockStoreLibrary.HostCommandEditLabel(rejectedCommand)} block=({rejectedCommand.A},{rejectedCommand.B},{rejectedCommand.C},{rejectedCommand.D})");
            return;
        }

        if (report.Result != 0)
        {
            Octaryn.Server.LiveDebugLog.Write($"server_live_client_commands_submit result={report.Result} reason={NativeBlockStoreLibrary.ClientBlockCommandSubmitReasonLabel(report.Reason)}");
            return;
        }

        var requestedCount = checked((int)report.RequestedCount);
        for (var index = 0; index < requestedCount; index++)
        {
            var command = commands[index];
            Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_queued index={index} kind={command.Kind} request={command.RequestId} edit={NativeBlockStoreLibrary.HostCommandEditLabel(command)} block=({command.A},{command.B},{command.C},{command.D})");
        }

        Octaryn.Server.LiveDebugLog.Write($"server_live_client_commands_submit result=0 pending_after={report.PendingAfter}");
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint IsClientPlaceable(void* context, ushort block)
    {
        return TryGetQueue(context, out var queue) &&
            queue._authorityRules.IsClientPlaceable(new BlockId(block))
                ? 1u
                : 0u;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint CanApplyCommand(void* context, HostCommand* command)
    {
        return TryGetQueue(context, out var queue) &&
            command is not null &&
            (queue._canPlaceAgainstPlayer is null || queue._canPlaceAgainstPlayer(*command)) &&
            queue._blockEdits.CanApplyCommand(*command)
                ? 1u
                : 0u;
    }

    private static bool TryGetQueue(void* context, out ClientBlockCommandQueue queue)
    {
        queue = null!;
        if (context is null)
        {
            return false;
        }

        var handle = GCHandle.FromIntPtr((IntPtr)context);
        if (handle.Target is not ClientBlockCommandQueue target)
        {
            return false;
        }

        queue = target;
        return true;
    }
}
