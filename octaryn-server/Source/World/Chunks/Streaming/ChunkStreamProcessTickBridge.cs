using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static unsafe class ChunkStreamProcessTickBridge
{
    public static int Execute(
        ModuleActivator gameModule,
        in HostFrameSnapshot frame,
        NativeChunkStreamProcessTickDecision decision)
    {
        var actions = new ProcessTickActions(gameModule);
        var actionHandle = GCHandle.Alloc(actions);
        var processFrame = frame;
        try
        {
            var context = (void*)GCHandle.ToIntPtr(actionHandle);
            var result = NativeBlockStoreLibrary.ChunkStreamExecuteProcessTick(
                &decision,
                &processFrame,
                &ExecuteHostOnlyProcessTick,
                &ExecuteFullProcessTick,
                context);
            if (actions.Exception is not null)
            {
                ExceptionDispatchInfo.Capture(actions.Exception).Throw();
            }

            return result;
        }
        finally
        {
            actionHandle.Free();
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int ExecuteHostOnlyProcessTick(void* context, HostFrameSnapshot* frame)
    {
        return ExecuteProcessTickAction(context, frame, hostOnly: true);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int ExecuteFullProcessTick(void* context, HostFrameSnapshot* frame)
    {
        return ExecuteProcessTickAction(context, frame, hostOnly: false);
    }

    private static int ExecuteProcessTickAction(void* context, HostFrameSnapshot* frame, bool hostOnly)
    {
        if (context is null || frame is null)
        {
            return -1;
        }

        try
        {
            var handle = GCHandle.FromIntPtr((IntPtr)context);
            return handle.Target is ProcessTickActions actions
                ? actions.Execute(frame, hostOnly)
                : -1;
        }
        catch
        {
            return -2;
        }
    }

    private sealed class ProcessTickActions(ModuleActivator gameModule)
    {
        public Exception? Exception { get; private set; }

        public int Execute(HostFrameSnapshot* frame, bool hostOnly)
        {
            try
            {
                var snapshot = *frame;
                if (hostOnly)
                {
                    gameModule.TickHostOnly(in snapshot);
                }
                else
                {
                    gameModule.Tick(in snapshot);
                }

                return 0;
            }
            catch (Exception exception)
            {
                Exception = exception;
                return -2;
            }
        }
    }
}
