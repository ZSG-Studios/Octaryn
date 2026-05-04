using Octaryn.Shared.Host;

internal static partial class SchedulerProbe
{
    private static void ValidateDefaultCapacity(string owner, Func<IHostScheduler> createScheduler)
    {
        using var disposable = (IDisposable)createScheduler();
        var scheduler = (IHostScheduler)disposable;
        var expected = Math.Max(
            HostSchedulingContract.MinimumWorkerThreads,
            Environment.ProcessorCount);
        if (scheduler.WorkerThreadCapacity != expected)
        {
            throw new InvalidOperationException(
                $"{owner}: default worker capacity {scheduler.WorkerThreadCapacity} did not match {expected}.");
        }
    }

    private static void ValidateTopology(
        string owner,
        HostSchedulerDiagnostics diagnostics,
        int workerThreadCapacity)
    {
        var expectedCoordinator = $"octaryn.{owner}.coordinator";
        if (diagnostics.CoordinatorThreadName != expectedCoordinator || !diagnostics.IsCoordinatorThreadAlive)
        {
            throw new InvalidOperationException($"{owner}: coordinator diagnostics are invalid.");
        }

        if (diagnostics.LiveWorkerThreadCount != workerThreadCapacity ||
            diagnostics.WorkerThreadNames.Count != workerThreadCapacity)
        {
            throw new InvalidOperationException($"{owner}: worker diagnostics do not match capacity.");
        }

        for (var index = 0; index < workerThreadCapacity; index++)
        {
            var expectedWorker = $"octaryn.{owner}.worker.{index}";
            if (!diagnostics.WorkerThreadNames.Contains(expectedWorker, StringComparer.Ordinal))
            {
                throw new InvalidOperationException($"{owner}: missing worker thread {expectedWorker}.");
            }
        }
    }

    private static void ValidateInvalidWorkerCounts(
        string owner,
        Func<int, IDisposable> createScheduler)
    {
        foreach (var workerCount in new[] { 0, 1 })
        {
            try
            {
                using var scheduler = createScheduler(workerCount);
                throw new InvalidOperationException($"{owner}: accepted invalid worker count {workerCount}.");
            }
            catch (ArgumentOutOfRangeException)
            {
            }
        }
    }

    private static void ValidateShutdownUnresolvedWorkDiagnostics<TScheduler>(
        string owner,
        Func<TScheduler> createScheduler,
        Func<TScheduler, HostSchedulerDiagnostics> getDiagnostics)
        where TScheduler : IHostScheduler, IDisposable
    {
        var scheduler = createScheduler();
        var unresolved = new HostScheduledWork(
            $"{owner}.probe.shutdown_unresolved",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ => throw new InvalidOperationException($"{owner}: unresolved shutdown work should not execute."));

        if (!scheduler.TrySchedule(unresolved))
        {
            throw new InvalidOperationException($"{owner}: unresolved shutdown work was rejected.");
        }

        scheduler.Dispose();
        var diagnostics = getDiagnostics(scheduler);
        if (diagnostics.FireAndForgetFailureCount != 1 ||
            diagnostics.LastFireAndForgetFailureWorkId != unresolved.WorkId ||
            diagnostics.LastFireAndForgetFailureType != typeof(InvalidOperationException).FullName)
        {
            throw new InvalidOperationException($"{owner}: unresolved shutdown failure diagnostics were not recorded.");
        }
    }

    private static void ValidateDisposedScheduler(string owner, IDisposable disposableScheduler)
    {
        var scheduler = (IHostScheduler)disposableScheduler;
        disposableScheduler.Dispose();
        if (scheduler.IsWorkerPoolAvailable)
        {
            throw new InvalidOperationException($"{owner}: disposed scheduler still reports worker availability.");
        }

        var work = new HostScheduledWork(
            $"{owner}.probe.rejected_after_dispose",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.None,
            _ => throw new InvalidOperationException($"{owner}: disposed scheduler executed work."));

        if (scheduler.TrySchedule(work) || scheduler.TryRun(work, CreateFrame(1)))
        {
            throw new InvalidOperationException($"{owner}: disposed scheduler accepted work.");
        }
    }

    private static HostFrameContext CreateFrame(ulong frameIndex)
    {
        return new HostFrameContext(
            1.0 / 60.0,
            frameIndex,
            default);
    }
}
