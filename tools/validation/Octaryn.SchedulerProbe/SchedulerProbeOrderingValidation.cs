using System.Collections.Concurrent;
using Octaryn.Shared.Host;

internal static partial class SchedulerProbe
{
    private static void ValidateBlockingTryRunOrderedDependencyRejected(string owner, IHostScheduler scheduler)
    {
        var prerequisite = new HostScheduledWork(
            $"{owner}.probe.order.after.prerequisite",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ => throw new InvalidOperationException($"{owner}: ordered blocking prerequisite should not execute."));
        var dependent = new HostScheduledWork(
            $"{owner}.probe.order.after.dependent",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ => throw new InvalidOperationException($"{owner}: ordered blocking dependent should not execute."));
        if (scheduler.TryRun(prerequisite, CreateFrame(91)) || scheduler.TryRun(dependent, CreateFrame(91)))
        {
            throw new InvalidOperationException($"{owner}: ordered blocking TryRun was accepted.");
        }
    }


    private static void ValidateRunsAfterFailedPrerequisiteSkipsDependent(string owner, IHostScheduler scheduler)
    {
        using var dependentStarted = new ManualResetEventSlim();
        using var prerequisiteStarted = new ManualResetEventSlim();

        var dependent = new HostScheduledWork(
            $"{owner}.probe.order.failed.dependent",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ => dependentStarted.Set());
        var prerequisite = new HostScheduledWork(
            $"{owner}.probe.order.failed.prerequisite",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                prerequisiteStarted.Set();
                throw new InvalidOperationException($"{owner}: expected ordered prerequisite failure.");
            });

        if (!scheduler.TrySchedule(dependent) || !scheduler.TrySchedule(prerequisite))
        {
            throw new InvalidOperationException($"{owner}: failed prerequisite order probe was rejected.");
        }

        if (!prerequisiteStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: failed prerequisite order probe did not start.");
        }

        if (dependentStarted.Wait(TimeSpan.FromMilliseconds(250)))
        {
            throw new InvalidOperationException($"{owner}: dependent ran after ordered prerequisite failure.");
        }
    }


    private static void ValidateRunsAfterResetsBetweenScheduleEpochs(string owner, IHostScheduler scheduler)
    {
        RunOrderedPairEpoch(owner, scheduler, "first");
        RunOrderedPairEpoch(owner, scheduler, "second");
    }

    private static void RunOrderedPairEpoch(string owner, IHostScheduler scheduler, string epoch)
    {
        using var dependentStarted = new ManualResetEventSlim();
        using var prerequisiteStarted = new ManualResetEventSlim();
        using var releasePrerequisite = new ManualResetEventSlim();

        var dependent = new HostScheduledWork(
            $"{owner}.probe.order.epoch.dependent",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ => dependentStarted.Set());
        var prerequisite = new HostScheduledWork(
            $"{owner}.probe.order.epoch.prerequisite",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                prerequisiteStarted.Set();
                if (!releasePrerequisite.Wait(TimeSpan.FromSeconds(5)))
                {
                    throw new InvalidOperationException($"{owner}: RunsAfter epoch {epoch} prerequisite release timed out.");
                }
            });

        if (!scheduler.TrySchedule(dependent))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter epoch {epoch} dependent work was rejected.");
        }

        if (dependentStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter epoch {epoch} reused stale prerequisite completion.");
        }

        if (!scheduler.TrySchedule(prerequisite))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter epoch {epoch} prerequisite work was rejected.");
        }

        if (!prerequisiteStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter epoch {epoch} prerequisite did not start.");
        }

        if (dependentStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter epoch {epoch} dependent started before prerequisite completed.");
        }

        releasePrerequisite.Set();
        if (!dependentStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter epoch {epoch} dependent did not start.");
        }
    }


    private static void ValidateCommitBarrierDrainsEarlierParallelWork(string owner, IHostScheduler scheduler)
    {
        using var parallelStarted = new ManualResetEventSlim();
        using var releaseParallel = new ManualResetEventSlim();
        using var barrierStarted = new ManualResetEventSlim();

        var parallel = new HostScheduledWork(
            $"{owner}.probe.barrier.parallel",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                parallelStarted.Set();
                if (!releaseParallel.Wait(TimeSpan.FromSeconds(5)))
                {
                    throw new InvalidOperationException($"{owner}: barrier parallel release timed out.");
                }
            });
        var barrier = new HostScheduledWork(
            $"{owner}.probe.barrier.commit",
            HostWorkPhase.Gameplay,
            HostWorkAccess.None,
            HostWorkScheduleFlags.RequiresTickBarrier,
            _ => barrierStarted.Set());

        if (!scheduler.TrySchedule(parallel))
        {
            throw new InvalidOperationException($"{owner}: barrier parallel work was rejected.");
        }

        if (!parallelStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: barrier parallel work did not start.");
        }

        if (!scheduler.TrySchedule(barrier))
        {
            throw new InvalidOperationException($"{owner}: barrier work was rejected.");
        }

        if (barrierStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: barrier work started before earlier parallel work completed.");
        }

        releaseParallel.Set();
        if (!barrierStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: barrier work did not start after earlier parallel work completed.");
        }
    }


    private static void ValidateRunsAfterScheduling(string owner, IHostScheduler scheduler)
    {
        using var dependentStarted = new ManualResetEventSlim();
        using var prerequisiteStarted = new ManualResetEventSlim();
        using var releasePrerequisite = new ManualResetEventSlim();
        var order = new ConcurrentQueue<string>();

        var dependent = new HostScheduledWork(
            $"{owner}.probe.order.after.dependent",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                order.Enqueue("dependent");
                dependentStarted.Set();
            });
        var prerequisite = new HostScheduledWork(
            $"{owner}.probe.order.after.prerequisite",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                order.Enqueue("prerequisite");
                prerequisiteStarted.Set();
                if (!releasePrerequisite.Wait(TimeSpan.FromSeconds(5)))
                {
                    throw new InvalidOperationException($"{owner}: RunsAfter prerequisite release timed out.");
                }
            });

        if (!scheduler.TrySchedule(dependent))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter dependent work was rejected.");
        }

        if (dependentStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter dependent started before prerequisite was scheduled.");
        }

        if (!scheduler.TrySchedule(prerequisite))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter prerequisite work was rejected.");
        }

        if (!prerequisiteStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter prerequisite did not start.");
        }

        if (dependentStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter dependent started before prerequisite completed.");
        }

        releasePrerequisite.Set();
        if (!dependentStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: RunsAfter dependent did not start after prerequisite.");
        }

        ValidateOrder(owner, "RunsAfter", order, "prerequisite", "dependent");
    }

    private static void ValidateRunsBeforeScheduling(string owner, IHostScheduler scheduler)
    {
        using var afterStarted = new ManualResetEventSlim();
        using var beforeStarted = new ManualResetEventSlim();
        using var releaseBefore = new ManualResetEventSlim();
        var order = new ConcurrentQueue<string>();

        var after = new HostScheduledWork(
            $"{owner}.probe.order.before.after",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                order.Enqueue("after");
                afterStarted.Set();
            });
        var before = new HostScheduledWork(
            $"{owner}.probe.order.before.before",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                order.Enqueue("before");
                beforeStarted.Set();
                if (!releaseBefore.Wait(TimeSpan.FromSeconds(5)))
                {
                    throw new InvalidOperationException($"{owner}: RunsBefore release timed out.");
                }
            });

        if (!scheduler.TrySchedule(after))
        {
            throw new InvalidOperationException($"{owner}: RunsBefore dependent work was rejected.");
        }

        if (afterStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: RunsBefore dependent started before prerequisite was scheduled.");
        }

        if (!scheduler.TrySchedule(before))
        {
            throw new InvalidOperationException($"{owner}: RunsBefore prerequisite work was rejected.");
        }

        if (!beforeStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: RunsBefore prerequisite did not start.");
        }

        if (afterStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: RunsBefore dependent started before prerequisite completed.");
        }

        releaseBefore.Set();
        if (!afterStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: RunsBefore dependent did not start after prerequisite.");
        }

        ValidateOrder(owner, "RunsBefore", order, "before", "after");
    }

    private static void ValidateOrder(
        string owner,
        string label,
        ConcurrentQueue<string> order,
        string expectedFirst,
        string expectedSecond)
    {
        if (!order.TryDequeue(out var first) ||
            !order.TryDequeue(out var second) ||
            first != expectedFirst ||
            second != expectedSecond)
        {
            throw new InvalidOperationException($"{owner}: {label} order was not enforced.");
        }
    }
}
