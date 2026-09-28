using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Time;
using Octaryn.Shared.Host;
using Octaryn.Shared.Time;
using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;

namespace Octaryn.Server.Tick;

internal sealed unsafe class AuthorityTickRunner : IDisposable
{
    private readonly NativeScheduleRuntime _scheduleRuntime;
    private readonly AuthorityTickActions _actions;
    private GCHandle _actionHandle;
    private readonly AuthorityTickProfile _profile = new();

    public AuthorityTickRunner(NativeScheduleRuntime scheduleRuntime,
        PlayerController playerController, WorldTimeClock worldTime)
    {
        _scheduleRuntime = scheduleRuntime;
        _actions = new AuthorityTickActions(playerController, worldTime, _profile);
        _actionHandle = GCHandle.Alloc(_actions);
    }

    public void Dispose()
    {
        if (_actionHandle.IsAllocated) _actionHandle.Free();
        _profile.Dispose();
    }

    public WorldTime Execute(in HostFrameContext frame, Func<int> drainClientCommands, out int appliedClientCommands)
    {
        ObjectDisposedException.ThrowIf(!_actionHandle.IsAllocated, this);
        var actions = _actions;
        actions.Reset(frame, drainClientCommands);
        {
            var context = (void*)GCHandle.ToIntPtr(_actionHandle);
            var callbacks = new NativeAuthorityTickCallbacks(
                &ExecuteCommandDrain,
                context,
                &ExecutePlayerTick,
                context,
                &ExecuteWorldTimeTick,
                context);
            var report = default(NativeScheduleRuntimeReport);
            _profile.BeginSchedule();
            var started = System.Diagnostics.Stopwatch.GetTimestamp();
            var result = NativeAuthorityTickLibrary.Execute(_scheduleRuntime.Handle, &callbacks, &report);
            _profile.Publish(frame.FrameIndex, started);
            if (actions.Exception is not null)
            {
                ExceptionDispatchInfo.Capture(actions.Exception).Throw();
            }

            if (result != 0)
            {
                throw new InvalidOperationException($"Native authority tick failed with result {result}.");
            }

            if (NativeAuthorityTickLibrary.ValidateReport(&report) != 0)
            {
                throw new InvalidOperationException("Native authority tick reported an unexpected schedule.");
            }

            appliedClientCommands = actions.AppliedClientCommands;
            return actions.WorldTime;
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int ExecuteCommandDrain(void* context)
    {
        return ExecuteAction(context, static actions => actions.ExecuteCommandDrain());
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
        WorldTimeClock worldTime, AuthorityTickProfile profile)
    {
        private HostFrameContext _frame;
        private Func<int> _drainClientCommands = null!;

        public void Reset(in HostFrameContext frame, Func<int> drainClientCommands)
        {
            _frame = frame;
            _drainClientCommands = drainClientCommands;
            AppliedClientCommands = 0;
            WorldTime = default;
            Exception = null;
        }

        public int AppliedClientCommands { get; private set; }

        public WorldTime WorldTime { get; private set; }

        public Exception? Exception { get; private set; }

        public int ExecuteCommandDrain()
        {
            profile.Begin();
            try
            {
                AppliedClientCommands = _drainClientCommands();
                return 0;
            }
            catch (Exception exception)
            {
                Exception = exception;
                return -2;
            }
            finally { profile.End(0); }
        }

        public int ExecutePlayerTick()
        {
            profile.Begin();
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
            finally { profile.End(1); }
        }

        public int ExecuteWorldTimeTick()
        {
            profile.Begin();
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
            finally { profile.End(2); }
        }
    }
}
