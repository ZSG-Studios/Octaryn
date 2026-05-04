using Octaryn.Shared.Host;

namespace Octaryn.Server.Tick;

internal sealed partial class HostScheduler
{
    private void RunWorker(int workerIndex)
    {
        isSchedulerThread = true;
        try
        {
            foreach (var scheduled in _workerQueue.GetConsumingEnumerable())
            {
                var context = new HostScheduledWorkContext(
                    scheduled.Frame,
                    HostThreadRole.WorkerPool,
                    workerIndex,
                    Volatile.Read(ref _isDisposed) != 0);
                try
                {
                    using var resourceScope = AcquireResourceScope(scheduled.Work);
                    using var commandWriteScope = HostCommandWriteScope.Enter(scheduled.Work.Access);
                    scheduled.Work.Execute(context);
                }
                catch (Exception exception)
                {
                    scheduled.Fail(exception);
                    if (!scheduled.IsBlocking)
                    {
                        RecordFireAndForgetFailure(scheduled.Work, exception);
                    }
                }
                finally
                {
                    scheduled.Complete();
                }
            }
        }
        finally
        {
            isSchedulerThread = false;
        }
    }

    private ResourceAccessScope AcquireResourceScope(HostScheduledWork work)
    {
        return _declaredSystems.TryGetValue(work.WorkId, out var declaration)
            ? ResourceAccessScope.Enter(_resourceLocks, declaration)
            : ResourceAccessScope.Empty;
    }

    private void RecordFireAndForgetFailure(HostScheduledWork work, Exception exception)
    {
        Volatile.Write(ref _lastFireAndForgetFailureWorkId, work.WorkId);
        Volatile.Write(ref _lastFireAndForgetFailureType, exception.GetType().FullName ?? exception.GetType().Name);
        Interlocked.Increment(ref _fireAndForgetFailureCount);
    }
}
