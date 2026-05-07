using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class ClientBlockCommandQueue : IDisposable
{
    private readonly BlockCommandSink _blockCommands;
    private readonly IBlockAuthorityRules _authorityRules;
    private IntPtr _handle;

    public ClientBlockCommandQueue(BlockCommandSink blockCommands, IBlockAuthorityRules authorityRules)
    {
        _blockCommands = blockCommands;
        _authorityRules = authorityRules;
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

    public int Drain()
    {
        var report = _blockCommands.DrainNativeClientCommands(Handle);
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
            queue._blockCommands.CanEnqueue(*command)
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
