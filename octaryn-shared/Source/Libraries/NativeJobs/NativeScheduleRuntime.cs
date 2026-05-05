using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host;

internal sealed unsafe class NativeScheduleRuntime : IDisposable
{
    private const uint MainThreadJob = 1u;
    private const uint CommandWriteJob = 1u << 2;

    private IntPtr _handle;

    public NativeScheduleRuntime()
    {
        _handle = NativeJobsLibrary.CreateScheduleRuntime(Environment.ProcessorCount, configuredWorkerLimit: 0);
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native schedule runtime allocation failed.");
        }
    }

    ~NativeScheduleRuntime()
    {
        Dispose();
    }

    public NativeScheduleRuntimeReport ExecuteCommandWriteMainThread(string jobId, Action action)
    {
        ObjectDisposedException.ThrowIf(_handle == IntPtr.Zero, this);
        var jobIdPointer = Marshal.StringToCoTaskMemUTF8(jobId);
        var scheduledAction = new ScheduledAction(action);
        var actionHandle = GCHandle.Alloc(scheduledAction);
        try
        {
            var job = new NativeScheduleRuntimeJob(
                (byte*)jobIdPointer,
                accesses: null,
                accessCount: 0,
                runsAfter: null,
                runsAfterCount: 0,
                MainThreadJob | CommandWriteJob,
                &ExecuteAction,
                (void*)GCHandle.ToIntPtr(actionHandle));
            var report = default(NativeScheduleRuntimeReport);
            var result = NativeJobsLibrary.ExecuteScheduleRuntime(_handle, &job, 1, &report);
            if (scheduledAction.Exception is not null)
            {
                ExceptionDispatchInfo.Capture(scheduledAction.Exception).Throw();
            }

            if (result != 0)
            {
                throw new InvalidOperationException($"Native schedule runtime job failed with result {result}.");
            }

            if (report.CompletedJobs != 1 ||
                report.MainThreadJobs != 1 ||
                report.WorkerJobs != 0 ||
                report.FailedJobIndex != -1)
            {
                throw new InvalidOperationException("Native schedule runtime reported an unexpected module tick route.");
            }

            return report;
        }
        finally
        {
            actionHandle.Free();
            Marshal.FreeCoTaskMem(jobIdPointer);
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
        NativeJobsLibrary.DestroyScheduleRuntime(handle);
        GC.SuppressFinalize(this);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int ExecuteAction(void* context)
    {
        if (context is null)
        {
            return -1;
        }

        try
        {
            var handle = GCHandle.FromIntPtr((IntPtr)context);
            if (handle.Target is not ScheduledAction scheduledAction)
            {
                return -1;
            }

            return scheduledAction.Execute();
        }
        catch
        {
            return -2;
        }
    }

    private sealed class ScheduledAction(Action action)
    {
        public Exception? Exception { get; private set; }

        public int Execute()
        {
            try
            {
                action();
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

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 64)]
internal readonly unsafe struct NativeScheduleRuntimeJob(
    byte* jobId,
    void* accesses,
    nuint accessCount,
    byte** runsAfter,
    nuint runsAfterCount,
    uint flags,
    delegate* unmanaged[Cdecl]<void*, int> execute,
    void* context)
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 64u;

    public readonly byte* JobId = jobId;
    public readonly void* Accesses = accesses;
    public readonly nuint AccessCount = accessCount;
    public readonly byte** RunsAfter = runsAfter;
    public readonly nuint RunsAfterCount = runsAfterCount;
    public readonly uint Flags = flags;
    public readonly delegate* unmanaged[Cdecl]<void*, int> Execute = execute;
    public readonly void* Context = context;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 48)]
internal struct NativeScheduleRuntimeReport
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 48u;

    public nuint SubmittedJobs;
    public nuint CompletedJobs;
    public nuint WorkerJobs;
    public nuint MainThreadJobs;
    public nuint ExecutionWaves;
    public int FailedJobIndex;
}
