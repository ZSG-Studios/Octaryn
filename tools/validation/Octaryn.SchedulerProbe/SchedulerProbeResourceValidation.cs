using System.Collections.Concurrent;
using Octaryn.Shared.Host;

internal static partial class SchedulerProbe
{
    private static void ValidateExactResourceWriteConflict(string owner, IHostScheduler scheduler)
    {
        using var firstStarted = new ManualResetEventSlim();
        using var releaseFirst = new ManualResetEventSlim();
        using var secondStarted = new ManualResetEventSlim();

        var first = new HostScheduledWork(
            $"{owner}.probe.resource_conflict.first",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ =>
            {
                firstStarted.Set();
                if (!releaseFirst.Wait(TimeSpan.FromSeconds(5)))
                {
                    throw new InvalidOperationException($"{owner}: exact conflict first release timed out.");
                }
            });
        var second = new HostScheduledWork(
            $"{owner}.probe.resource_conflict.second",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.CanRunInParallel,
            _ => secondStarted.Set());

        if (!scheduler.TrySchedule(first))
        {
            throw new InvalidOperationException($"{owner}: exact conflict first work was rejected.");
        }

        if (!firstStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: exact conflict first work did not start.");
        }

        if (!scheduler.TrySchedule(second))
        {
            throw new InvalidOperationException($"{owner}: exact conflict second work was rejected.");
        }

        if (secondStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: exact conflicting resource work ran concurrently.");
        }

        releaseFirst.Set();
        if (!secondStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: exact conflict second work did not complete.");
        }
    }

    private static void ValidateIndependentResourceWritesUseWorkerPool(string owner, IHostScheduler scheduler)
    {
        using var bothStarted = new CountdownEvent(2);
        using var releaseWorkers = new ManualResetEventSlim();

        for (var index = 0; index < 2; index++)
        {
            var resourceIndex = index;
            var work = new HostScheduledWork(
                $"{owner}.probe.resource_independent.{resourceIndex}",
                HostWorkPhase.Gameplay,
                HostWorkAccess.GameplayStateWrite,
                HostWorkScheduleFlags.CanRunInParallel,
                context =>
                {
                    if (context.ThreadRole != HostThreadRole.WorkerPool || context.WorkerIndex < 0)
                    {
                        throw new InvalidOperationException($"{owner}: independent resource work did not run on worker pool.");
                    }

                    bothStarted.Signal();
                    if (!releaseWorkers.Wait(TimeSpan.FromSeconds(5)))
                    {
                        throw new InvalidOperationException($"{owner}: independent resource release timed out.");
                    }
                });

            if (!scheduler.TrySchedule(work))
            {
                throw new InvalidOperationException($"{owner}: independent resource work was rejected.");
            }
        }

        if (!bothStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: independent resource writes did not run in parallel.");
        }

        releaseWorkers.Set();
    }

    private static void ValidateSerialScheduling(string owner, IHostScheduler scheduler)
    {
        using var firstStarted = new ManualResetEventSlim();
        using var releaseFirst = new ManualResetEventSlim();
        using var secondStarted = new ManualResetEventSlim();
        var order = new ConcurrentQueue<string>();

        var first = new HostScheduledWork(
            $"{owner}.probe.serial.first",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
            _ =>
            {
                order.Enqueue("first");
                firstStarted.Set();
                if (!releaseFirst.Wait(TimeSpan.FromSeconds(5)))
                {
                    throw new InvalidOperationException($"{owner}: serial first release timed out.");
                }
            });
        var second = new HostScheduledWork(
            $"{owner}.probe.serial.second",
            HostWorkPhase.Gameplay,
            HostWorkAccess.GameplayStateWrite,
            HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
            _ =>
            {
                order.Enqueue("second");
                secondStarted.Set();
            });

        if (!scheduler.TrySchedule(first) || !scheduler.TrySchedule(second))
        {
            throw new InvalidOperationException($"{owner}: serial work was rejected.");
        }

        if (!firstStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: serial first work did not start.");
        }

        if (secondStarted.Wait(TimeSpan.FromMilliseconds(150)))
        {
            throw new InvalidOperationException($"{owner}: serial second work started before first completed.");
        }

        releaseFirst.Set();
        if (!secondStarted.Wait(TimeSpan.FromSeconds(5)))
        {
            throw new InvalidOperationException($"{owner}: serial second work did not start.");
        }

        if (!order.TryDequeue(out var firstOrder) ||
            !order.TryDequeue(out var secondOrder) ||
            firstOrder != "first" ||
            secondOrder != "second")
        {
            throw new InvalidOperationException($"{owner}: serial work order was not deterministic.");
        }
    }
}
