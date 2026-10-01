using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Modules;

// One synchronous owner-thread callback for the lifetime of a module instance.
internal sealed unsafe class ModuleTickCall : IDisposable
{
    private readonly NativeScheduleRuntime _runtime;
    private readonly Callback _callback;
    private GCHandle _context;
    private IntPtr _name;
    private int _state; // idle, executing, disposed

    public ModuleTickCall(NativeScheduleRuntime runtime, Action action)
    {
        _runtime = runtime;
        _callback = new Callback(action);
        _name = Marshal.StringToCoTaskMemUTF8("server.module.tick");
        try { _context = GCHandle.Alloc(_callback); }
        catch { Marshal.FreeCoTaskMem(_name); _name = IntPtr.Zero; throw; }
    }

    public void Execute()
    {
        var previous = Interlocked.CompareExchange(ref _state, 1, 0);
        ObjectDisposedException.ThrowIf(previous == 2, this);
        if (previous != 0) throw new InvalidOperationException("Module tick callback is already executing.");
        try
        {
            _callback.Exception = null;
            var job = new NativeScheduleRuntimeJob((byte*)_name, null, 0, null, 0,
                flags: 1u | (1u << 2), &Invoke, (void*)GCHandle.ToIntPtr(_context));
            var report = default(NativeScheduleRuntimeReport);
            var result = NativeJobsLibrary.ExecuteScheduleRuntime(_runtime.Handle, &job, 1, &report);
            if (_callback.Exception is { } exception) ExceptionDispatchInfo.Capture(exception).Throw();
            if (result != 0) throw new InvalidOperationException($"Module tick schedule failed: {result}.");
            if (report.SubmittedJobs != 1 || report.CompletedJobs != 1 || report.MainThreadJobs != 1 ||
                report.WorkerJobs != 0 || report.ExecutionWaves != 1 || report.FailedJobIndex != -1)
                throw new InvalidOperationException("Module tick schedule route changed.");
        }
        finally { Volatile.Write(ref _state, 0); }
    }

    public void Dispose()
    {
        var previous = Interlocked.CompareExchange(ref _state, 2, 0);
        if (previous == 2) return;
        if (previous != 0) throw new InvalidOperationException("Cannot dispose an executing module tick callback.");
        _context.Free();
        Marshal.FreeCoTaskMem(_name);
        _name = IntPtr.Zero;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int Invoke(void* context)
    {
        if (context is null) return -1;
        try
        {
            if (GCHandle.FromIntPtr((IntPtr)context).Target is not Callback callback) return -1;
            try { callback.Action(); return 0; }
            catch (Exception exception) { callback.Exception = exception; return -2; }
        }
        catch { return -2; }
    }

    private sealed class Callback(Action action)
    {
        public readonly Action Action = action;
        public Exception? Exception;
    }
}
