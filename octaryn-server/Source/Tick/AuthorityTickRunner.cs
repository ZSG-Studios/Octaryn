using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Time;
using Octaryn.Shared.Host;
using Octaryn.Shared.Time;
using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;

namespace Octaryn.Server.Tick;

internal sealed unsafe class AuthorityTickRunner(
    NativeScheduleRuntime scheduleRuntime,
    PlayerController playerController,
    WorldTimeClock worldTime)
{
    public WorldTime Execute(in HostFrameContext frame)
    {
        var actions = new AuthorityTickActions(playerController, worldTime, frame);
        var actionHandle = GCHandle.Alloc(actions);
        try
        {
            var context = (void*)GCHandle.ToIntPtr(actionHandle);
            var callbacks = new NativeAuthorityTickCallbacks(
                &ExecutePlayerTick,
                context,
                &ExecuteWorldTimeTick,
                context);
            var report = default(NativeScheduleRuntimeReport);
            var result = NativeAuthorityTickLibrary.Execute(scheduleRuntime.Handle, &callbacks, &report);
            if (actions.Exception is not null)
            {
                ExceptionDispatchInfo.Capture(actions.Exception).Throw();
            }

            if (result != 0)
            {
                throw new InvalidOperationException($"Native authority tick failed with result {result}.");
            }

            if (report.CompletedJobs != 2 ||
                report.WorkerJobs != 2 ||
                report.MainThreadJobs != 0 ||
                report.ExecutionWaves != 2 ||
                report.FailedJobIndex != -1)
            {
                throw new InvalidOperationException("Native authority tick reported an unexpected schedule.");
            }

            return actions.WorldTime;
        }
        finally
        {
            actionHandle.Free();
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int ExecutePlayerTick(void* context)
    {
        return ExecuteAction(context, static actions => actions.ExecutePlayerTick());
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int ExecuteWorldTimeTick(void* context)
    {
        return ExecuteAction(context, static actions => actions.ExecuteWorldTimeTick());
    }

    private static int ExecuteAction(void* context, Func<AuthorityTickActions, int> action)
    {
        if (context is null)
        {
            return -1;
        }

        try
        {
            var handle = GCHandle.FromIntPtr((IntPtr)context);
            return handle.Target is AuthorityTickActions actions ? action(actions) : -1;
        }
        catch
        {
            return -2;
        }
    }

    private sealed class AuthorityTickActions(
        PlayerController playerController,
        WorldTimeClock worldTime,
        HostFrameContext frame)
    {
        private readonly HostFrameContext _frame = frame;

        public WorldTime WorldTime { get; private set; }

        public Exception? Exception { get; private set; }

        public int ExecutePlayerTick()
        {
            try
            {
                playerController.Tick(in _frame);
                return 0;
            }
            catch (Exception exception)
            {
                Exception = exception;
                return -2;
            }
        }

        public int ExecuteWorldTimeTick()
        {
            try
            {
                WorldTime = worldTime.AdvanceFrame(_frame.DeltaSeconds);
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
