using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class ClientBlockCommandQueue : IDisposable
{
    public const int MaxPendingCommands = 4096;

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

    public bool Enqueue(HostCommand command)
    {
        var handle = GCHandle.Alloc(this);
        var queued = false;
        try
        {
            queued = NativeBlockStoreLibrary.ClientBlockCommandQueueEnqueue(
                Handle,
                &command,
                &IsClientPlaceable,
                &CanApplyCommand,
                (void*)GCHandle.ToIntPtr(handle)) != 0;
        }
        finally
        {
            handle.Free();
        }

        Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_queue queued={(queued ? 1 : 0)} pending={PendingCount} kind={command.Kind} request={command.RequestId} edit={BlockCommandDiagnostics.EditLabel(command)} block=({command.A},{command.B},{command.C},{command.D})");
        return queued;
    }

    public int Drain()
    {
        var handle = GCHandle.Alloc(this);
        int applied;
        try
        {
            applied = NativeBlockStoreLibrary.ClientBlockCommandQueueDrain(
                Handle,
                &ApplyCommand,
                (void*)GCHandle.ToIntPtr(handle));
        }
        finally
        {
            handle.Free();
        }

        Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_drain applied={applied} pending={PendingCount}");
        return applied;
    }

    internal bool CanQueue(HostCommand command)
    {
        var handle = GCHandle.Alloc(this);
        try
        {
            return NativeBlockStoreLibrary.ClientBlockCommandQueueCanQueue(
                Handle,
                &command,
                &IsClientPlaceable,
                &CanApplyCommand,
                (void*)GCHandle.ToIntPtr(handle)) != 0;
        }
        finally
        {
            handle.Free();
        }
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

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint ApplyCommand(void* context, HostCommand* command)
    {
        return TryGetQueue(context, out var queue) &&
            command is not null &&
            queue._blockCommands.Enqueue(*command)
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
