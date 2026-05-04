using System.Collections.Concurrent;
using Octaryn.Shared.Host;

internal static partial class SchedulerProbe
{
    private static void ValidateScheduler(
        string owner,
        IHostScheduler scheduler,
        Func<HostSchedulerDiagnostics> getDiagnostics)
    {
        if (scheduler.MinimumWorkerThreads != HostSchedulingContract.MinimumWorkerThreads)
        {
            throw new InvalidOperationException($"{owner}: wrong minimum worker count.");
        }

        if (scheduler.WorkerThreadCapacity < HostSchedulingContract.MinimumWorkerThreads)
        {
            throw new InvalidOperationException($"{owner}: worker capacity below contract minimum.");
        }

        if (!scheduler.IsWorkerPoolAvailable)
        {
            throw new InvalidOperationException($"{owner}: worker pool unavailable before disposal.");
        }

        var frame = CreateFrame(42);
        var ranBlockingWork = false;
        var blockingWork = new HostScheduledWork(
            $"{owner}.probe.blocking",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.RequiresTickBarrier,
            context =>
            {
                if (context.ThreadRole != HostThreadRole.WorkerPool)
                {
                    throw new InvalidOperationException($"{owner}: blocking work did not run on worker pool.");
                }

                if (context.WorkerIndex < 0)
                {
                    throw new InvalidOperationException($"{owner}: worker index was not assigned.");
                }

                if (context.Frame.FrameIndex != frame.FrameIndex)
                {
                    throw new InvalidOperationException($"{owner}: frame context was not propagated.");
                }

                ranBlockingWork = true;
            });

        if (!scheduler.TryRun(blockingWork, frame) || !ranBlockingWork)
        {
            throw new InvalidOperationException($"{owner}: blocking scheduled work did not complete.");
        }

        using var fireAndForgetCompleted = new ManualResetEventSlim();
        var fireAndForgetWork = new HostScheduledWork(
            $"{owner}.probe.fire_and_forget",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.None,
            context =>
            {
                if (context.ThreadRole != HostThreadRole.WorkerPool || context.WorkerIndex < 0)
                {
                    throw new InvalidOperationException($"{owner}: fire-and-forget work did not run on worker pool.");
                }

                fireAndForgetCompleted.Set();
            });

        if (!scheduler.TrySchedule(fireAndForgetWork))
        {
            throw new InvalidOperationException($"{owner}: fire-and-forget schedule was rejected.");
        }

        if (!fireAndForgetCompleted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: fire-and-forget work did not execute.");
        }

        var expectedFailure = new InvalidOperationException($"{owner}: expected blocking failure.");
        var failingBlockingWork = new HostScheduledWork(
            $"{owner}.probe.failing_blocking",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.RequiresTickBarrier,
            _ => throw expectedFailure);

        try
        {
            scheduler.TryRun(failingBlockingWork, frame);
            throw new InvalidOperationException($"{owner}: blocking scheduler failure did not propagate.");
        }
        catch (InvalidOperationException exception) when (ReferenceEquals(exception, expectedFailure))
        {
        }

        using var workerSurvived = new ManualResetEventSlim();
        var failingFireAndForgetWork = new HostScheduledWork(
            $"{owner}.probe.failing_fire_and_forget",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.None,
            _ => throw new InvalidOperationException($"{owner}: expected fire-and-forget failure."));
        var postFailureWork = new HostScheduledWork(
            $"{owner}.probe.after_fire_and_forget_failure",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.None,
            context =>
            {
                if (context.ThreadRole == HostThreadRole.Main)
                {
                    throw new InvalidOperationException($"{owner}: work executed on main thread.");
                }

                workerSurvived.Set();
            });

        if (!scheduler.TrySchedule(failingFireAndForgetWork) || !scheduler.TrySchedule(postFailureWork))
        {
            throw new InvalidOperationException($"{owner}: scheduler rejected fire-and-forget failure probe.");
        }

        if (!workerSurvived.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: worker pool stopped after fire-and-forget failure.");
        }

        var diagnosticsAfterFailure = getDiagnostics();
        if (diagnosticsAfterFailure.FireAndForgetFailureCount != 1 ||
            diagnosticsAfterFailure.LastFireAndForgetFailureWorkId != failingFireAndForgetWork.WorkId ||
            diagnosticsAfterFailure.LastFireAndForgetFailureType != typeof(InvalidOperationException).FullName)
        {
            throw new InvalidOperationException($"{owner}: fire-and-forget failure diagnostics were not recorded.");
        }

        ValidateSerialScheduling(owner, scheduler);
        ValidateRunsAfterScheduling(owner, scheduler);
        ValidateRunsAfterResetsBetweenScheduleEpochs(owner, scheduler);
        ValidateRunsAfterFailedPrerequisiteSkipsDependent(owner, scheduler);
        ValidateRunsBeforeScheduling(owner, scheduler);
        ValidateCommitBarrierDrainsEarlierParallelWork(owner, scheduler);
        ValidateExactResourceWriteConflict(owner, scheduler);
        ValidateIndependentResourceWritesUseWorkerPool(owner, scheduler);
        ValidateNestedBlockingRunRejected(owner, scheduler);
        ValidateUndeclaredWorkRejected(owner, scheduler);
        ValidateBlockingTryRunOrderedDependencyRejected(owner, scheduler);
        ValidateParallelCapacity(owner, scheduler);
    }

    private static void ValidateNestedBlockingRunRejected(string owner, IHostScheduler scheduler)
    {
        using var completed = new ManualResetEventSlim();
        var frame = CreateFrame(77);
        var nested = new HostScheduledWork(
            $"{owner}.probe.nested_blocking_inner",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.RequiresTickBarrier,
            _ => throw new InvalidOperationException($"{owner}: nested work should not execute."));
        var outer = new HostScheduledWork(
            $"{owner}.probe.nested_blocking_outer",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                if (scheduler.TryRun(nested, frame))
                {
                    throw new InvalidOperationException($"{owner}: nested blocking TryRun was accepted.");
                }

                completed.Set();
            });

        if (!scheduler.TrySchedule(outer))
        {
            throw new InvalidOperationException($"{owner}: nested blocking probe was rejected.");
        }

        if (!completed.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: nested blocking probe did not complete.");
        }
    }

    private static void ValidateParallelCapacity(string owner, IHostScheduler scheduler)
    {
        using var allStarted = new CountdownEvent(scheduler.WorkerThreadCapacity);
        using var releaseWorkers = new ManualResetEventSlim();
        var workers = new ConcurrentDictionary<int, byte>();

        for (var index = 0; index < scheduler.WorkerThreadCapacity; index++)
        {
            var work = new HostScheduledWork(
                $"{owner}.probe.parallel_capacity.{index}",
                HostWorkPhase.Validation,
                HostWorkAccess.None,
                HostWorkScheduleFlags.CanRunInParallel,
                context =>
                {
                    if (context.ThreadRole != HostThreadRole.WorkerPool || context.WorkerIndex < 0)
                    {
                        throw new InvalidOperationException($"{owner}: parallel work did not run on worker pool.");
                    }

                    workers.TryAdd(context.WorkerIndex, 0);
                    allStarted.Signal();
                    if (!releaseWorkers.Wait(TimeSpan.FromSeconds(5)))
                    {
                        throw new InvalidOperationException($"{owner}: parallel capacity release timed out.");
                    }
                });

            if (!scheduler.TrySchedule(work))
            {
                throw new InvalidOperationException($"{owner}: parallel capacity work was rejected.");
            }
        }

        if (!allStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: worker pool did not start capacity probe.");
        }

        releaseWorkers.Set();
        if (workers.Count != scheduler.WorkerThreadCapacity)
        {
            throw new InvalidOperationException($"{owner}: worker pool did not use full capacity.");
        }
    }

    private static void ValidateUndeclaredWorkRejected(string owner, IHostScheduler scheduler)
    {
        var undeclared = new HostScheduledWork(
            $"{owner}.probe.undeclared",
            HostWorkPhase.Validation,
            HostWorkAccess.None,
            HostWorkScheduleFlags.None,
            _ => throw new InvalidOperationException($"{owner}: undeclared work executed."));
        if (scheduler.TrySchedule(undeclared) || scheduler.TryRun(undeclared, CreateFrame(88)))
        {
            throw new InvalidOperationException($"{owner}: undeclared work was accepted.");
        }

        var mismatchedPhase = new HostScheduledWork(
            $"{owner}.probe.blocking",
            HostWorkPhase.Gameplay,
            HostWorkAccess.None,
            HostWorkScheduleFlags.RequiresTickBarrier,
            _ => throw new InvalidOperationException($"{owner}: mismatched work executed."));
        if (scheduler.TrySchedule(mismatchedPhase) || scheduler.TryRun(mismatchedPhase, CreateFrame(89)))
        {
            throw new InvalidOperationException($"{owner}: mismatched declared work was accepted.");
        }

        var mismatchedAccess = new HostScheduledWork(
            $"{owner}.probe.blocking",
            HostWorkPhase.Validation,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.RequiresTickBarrier,
            _ => throw new InvalidOperationException($"{owner}: access-mismatched work executed."));
        if (scheduler.TrySchedule(mismatchedAccess) || scheduler.TryRun(mismatchedAccess, CreateFrame(90)))
        {
            throw new InvalidOperationException($"{owner}: access-mismatched declared work was accepted.");
        }
    }
}
