using Octaryn.Shared.Host;

namespace Octaryn.Server.Tick;

internal sealed partial class HostScheduler
{
    private void CoordinateWork()
    {
        var pending = new List<ScheduledHostWork>();
        var inFlight = new List<ScheduledHostWork>();
        var completedWorkIds = new HashSet<string>(StringComparer.Ordinal);
        var failedWorkIds = new HashSet<string>(StringComparer.Ordinal);
        isSchedulerThread = true;
        try
        {
            foreach (var work in _coordinatorQueue.GetConsumingEnumerable())
            {
                pending.Add(work);
                DispatchReadyWork(pending, inFlight, completedWorkIds, failedWorkIds);
            }

            DrainInFlightWork(inFlight, completedWorkIds, failedWorkIds);
            CompleteUnresolvedWork(pending);
        }
        finally
        {
            isSchedulerThread = false;
        }
    }

    private static bool MustRunSerially(HostScheduledWork work)
    {
        if ((work.Flags & HostWorkScheduleFlags.CanRunInParallel) == 0)
        {
            return true;
        }

        return (work.Flags & (HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier)) != 0;
    }

    private void DispatchReadyWork(
        List<ScheduledHostWork> pending,
        List<ScheduledHostWork> inFlight,
        HashSet<string> completedWorkIds,
        HashSet<string> failedWorkIds)
    {
        var madeProgress = true;
        while (madeProgress)
        {
            madeProgress = false;
            for (var index = 0; index < pending.Count; index++)
            {
                var scheduled = pending[index];
                if (HasFailedPrerequisite(scheduled.Work.WorkId, failedWorkIds))
                {
                    pending.RemoveAt(index);
                    index--;
                    FailSkippedWork(scheduled, failedWorkIds);
                    madeProgress = true;
                    continue;
                }

                if (!ArePrerequisitesComplete(scheduled.Work.WorkId, completedWorkIds))
                {
                    continue;
                }

                pending.RemoveAt(index);
                index--;
                DispatchWork(scheduled, inFlight, completedWorkIds, failedWorkIds);
                madeProgress = true;
            }
        }

        ResetCompletedWorkIdsIfIdle(pending, inFlight, completedWorkIds, failedWorkIds);
    }

    private bool ArePrerequisitesComplete(string workId, IReadOnlySet<string> completedWorkIds)
    {
        return !_workPrerequisites.TryGetValue(workId, out var prerequisites) ||
            prerequisites.All(completedWorkIds.Contains);
    }

    private bool HasPrerequisites(string workId)
    {
        return _workPrerequisites.TryGetValue(workId, out var prerequisites) && prerequisites.Count > 0;
    }

    private bool IsOrderedWork(string workId)
    {
        return _orderedWorkIds.Contains(workId);
    }

    private bool HasFailedPrerequisite(string workId, IReadOnlySet<string> failedWorkIds)
    {
        return _workPrerequisites.TryGetValue(workId, out var prerequisites) &&
            prerequisites.Any(failedWorkIds.Contains);
    }

    private void DispatchWork(
        ScheduledHostWork scheduled,
        List<ScheduledHostWork> inFlight,
        HashSet<string> completedWorkIds,
        HashSet<string> failedWorkIds)
    {
        if (MustRunSerially(scheduled.Work) || _orderedWorkIds.Contains(scheduled.Work.WorkId))
        {
            DrainInFlightWork(inFlight, completedWorkIds, failedWorkIds);
            scheduled.MarkCoordinatorWaiter();
            _workerQueue.Add(scheduled);
            scheduled.WaitForCompletion();
            RecordWorkCompletion(scheduled, completedWorkIds, failedWorkIds);
            if (!scheduled.IsBlocking)
            {
                scheduled.Dispose();
            }

            return;
        }

        scheduled.MarkCoordinatorWaiter();
        _workerQueue.Add(scheduled);
        inFlight.Add(scheduled);
    }

    private static void DrainInFlightWork(
        List<ScheduledHostWork> inFlight,
        HashSet<string> completedWorkIds,
        HashSet<string> failedWorkIds)
    {
        foreach (var scheduled in inFlight)
        {
            scheduled.WaitForCompletion();
            RecordWorkCompletion(scheduled, completedWorkIds, failedWorkIds);
            if (!scheduled.IsBlocking)
            {
                scheduled.Dispose();
            }
        }

        inFlight.Clear();
    }

    private static void ResetCompletedWorkIdsIfIdle(
        IReadOnlyCollection<ScheduledHostWork> pending,
        IReadOnlyCollection<ScheduledHostWork> inFlight,
        HashSet<string> completedWorkIds,
        HashSet<string> failedWorkIds)
    {
        if (pending.Count == 0 && inFlight.Count == 0)
        {
            completedWorkIds.Clear();
            failedWorkIds.Clear();
        }
    }

    private static void RecordWorkCompletion(
        ScheduledHostWork scheduled,
        ISet<string> completedWorkIds,
        ISet<string> failedWorkIds)
    {
        if (scheduled.HasFailed)
        {
            failedWorkIds.Add(scheduled.Work.WorkId);
            return;
        }

        completedWorkIds.Add(scheduled.Work.WorkId);
    }

    private void FailSkippedWork(ScheduledHostWork scheduled, ISet<string> failedWorkIds)
    {
        var exception = new InvalidOperationException(
            $"Scheduled work prerequisite failed. Work: {scheduled.Work.WorkId}");
        scheduled.Fail(exception);
        scheduled.Complete();
        failedWorkIds.Add(scheduled.Work.WorkId);
        if (!scheduled.IsBlocking)
        {
            RecordFireAndForgetFailure(scheduled.Work, exception);
            scheduled.Dispose();
        }
    }

    private void CompleteUnresolvedWork(IReadOnlyList<ScheduledHostWork> pending)
    {
        foreach (var scheduled in pending)
        {
            var exception = new InvalidOperationException(
                $"Scheduled work dependencies were not satisfied before shutdown. Work: {scheduled.Work.WorkId}");
            scheduled.Fail(exception);
            scheduled.Complete();
            if (!scheduled.IsBlocking)
            {
                RecordFireAndForgetFailure(scheduled.Work, exception);
                scheduled.Dispose();
            }
        }
    }

    private static IReadOnlyDictionary<string, IReadOnlySet<string>> BuildWorkPrerequisites(
        IEnumerable<ScheduledSystemDeclaration> declarations)
    {
        var prerequisites = new Dictionary<string, HashSet<string>>(StringComparer.Ordinal);
        var declarationList = declarations.ToArray();
        foreach (var declaration in declarationList)
        {
            prerequisites.TryAdd(declaration.SystemId, new HashSet<string>(StringComparer.Ordinal));
        }

        foreach (var declaration in declarationList)
        {
            var current = prerequisites[declaration.SystemId];
            foreach (var dependency in declaration.RunsAfter)
            {
                current.Add(dependency);
            }

            foreach (var dependency in declaration.RunsBefore)
            {
                if (prerequisites.TryGetValue(dependency, out var dependent))
                {
                    dependent.Add(declaration.SystemId);
                }
            }
        }

        return prerequisites.ToDictionary(
            pair => pair.Key,
            pair => (IReadOnlySet<string>)pair.Value,
            StringComparer.Ordinal);
    }

    private static IReadOnlySet<string> BuildOrderedWorkIds(
        IReadOnlyDictionary<string, IReadOnlySet<string>> prerequisites)
    {
        var ordered = new HashSet<string>(StringComparer.Ordinal);
        foreach (var pair in prerequisites)
        {
            if (pair.Value.Count > 0)
            {
                ordered.Add(pair.Key);
            }

            foreach (var dependency in pair.Value)
            {
                ordered.Add(dependency);
            }
        }

        return ordered;
    }
}
