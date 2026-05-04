using Octaryn.Shared.Host;

namespace Octaryn.Server.Tick;

internal sealed class ScheduledHostWork(
    HostScheduledWork work,
    HostFrameContext frame,
    bool isBlocking)
    : IDisposable
{
    private readonly ManualResetEventSlim _completion = new();
    private Exception? _exception;
    private int _hasCoordinatorWaiter;

    public HostScheduledWork Work { get; } = work;

    public HostFrameContext Frame { get; } = frame;

    public bool IsBlocking { get; } = isBlocking;

    public bool HasFailed => _exception is not null;

    public static ScheduledHostWork FireAndForget(HostScheduledWork work)
    {
        return new ScheduledHostWork(work, default, false);
    }

    public static ScheduledHostWork Blocking(HostScheduledWork work, HostFrameContext frame)
    {
        return new ScheduledHostWork(work, frame, true);
    }

    public void MarkCoordinatorWaiter()
    {
        Volatile.Write(ref _hasCoordinatorWaiter, 1);
    }

    public void WaitForCompletion()
    {
        _completion.Wait();
    }

    public void Fail(Exception exception)
    {
        _exception = exception;
    }

    public void Complete()
    {
        _completion.Set();
        if (!IsBlocking && Volatile.Read(ref _hasCoordinatorWaiter) == 0)
        {
            Dispose();
        }
    }

    public void ThrowIfFailed()
    {
        if (_exception is not null)
        {
            throw _exception;
        }
    }

    public void Dispose()
    {
        _completion.Dispose();
    }
}
