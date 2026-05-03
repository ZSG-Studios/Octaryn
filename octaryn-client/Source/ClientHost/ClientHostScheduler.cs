using System.Collections.Concurrent;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;

namespace Octaryn.Client.ClientHost;

internal sealed class ClientHostScheduler : IHostScheduler, IDisposable
{
    [ThreadStatic]
    private static bool isSchedulerThread;

    private readonly BlockingCollection<ScheduledHostWork> _coordinatorQueue = [];
    private readonly BlockingCollection<ScheduledHostWork> _workerQueue = [];
    private readonly ConcurrentDictionary<string, ReaderWriterLockSlim> _resourceLocks = new(StringComparer.Ordinal);
    private readonly IReadOnlyDictionary<string, ScheduledSystemDeclaration> _declaredSystems;
    private readonly IReadOnlyDictionary<string, IReadOnlySet<string>> _workPrerequisites;
    private readonly IReadOnlySet<string> _orderedWorkIds;
    private readonly Thread _coordinatorThread;
    private readonly Thread[] _workerThreads;
    private long _fireAndForgetFailureCount;
    private string? _lastFireAndForgetFailureWorkId;
    private string? _lastFireAndForgetFailureType;
    private int _isDisposed;

    public ClientHostScheduler()
        : this(CreateDefaultWorkerCount())
    {
    }

    internal ClientHostScheduler(int workerThreadCapacity)
        : this(workerThreadCapacity, [])
    {
    }

    internal ClientHostScheduler(IReadOnlyList<ScheduledSystemDeclaration> declaredSystems)
        : this(CreateDefaultWorkerCount(), declaredSystems)
    {
    }

    internal ClientHostScheduler(
        int workerThreadCapacity,
        IReadOnlyList<ScheduledSystemDeclaration> declaredSystems)
    {
        if (!HostSchedulingContract.IsValidWorkerThreadCapacity(workerThreadCapacity))
        {
            throw new ArgumentOutOfRangeException(
                nameof(workerThreadCapacity),
                workerThreadCapacity,
                "Worker thread capacity must satisfy the host scheduling contract.");
        }

        _declaredSystems = declaredSystems.ToDictionary(system => system.SystemId, StringComparer.Ordinal);
        _workPrerequisites = BuildWorkPrerequisites(_declaredSystems.Values);
        _orderedWorkIds = BuildOrderedWorkIds(_workPrerequisites);
        WorkerThreadCapacity = workerThreadCapacity;
        _coordinatorThread = new Thread(CoordinateWork)
        {
            IsBackground = true,
            Name = "octaryn.client.coordinator"
        };
        _workerThreads = new Thread[WorkerThreadCapacity];
        for (var index = 0; index < _workerThreads.Length; index++)
        {
            var workerIndex = index;
            _workerThreads[index] = new Thread(() => RunWorker(workerIndex))
            {
                IsBackground = true,
                Name = $"octaryn.client.worker.{workerIndex}"
            };
        }

        _coordinatorThread.Start();
        foreach (var workerThread in _workerThreads)
        {
            workerThread.Start();
        }
    }

    public int MinimumWorkerThreads => HostSchedulingContract.MinimumWorkerThreads;

    public int WorkerThreadCapacity { get; }

    public bool IsWorkerPoolAvailable => Volatile.Read(ref _isDisposed) == 0;

    internal HostSchedulerDiagnostics Diagnostics => new(
        _coordinatorThread.Name ?? string.Empty,
        _coordinatorThread.IsAlive,
        _workerThreads.Select(thread => thread.Name ?? string.Empty).ToArray(),
        _workerThreads.Count(thread => thread.IsAlive),
        Interlocked.Read(ref _fireAndForgetFailureCount),
        Volatile.Read(ref _lastFireAndForgetFailureWorkId),
        Volatile.Read(ref _lastFireAndForgetFailureType));

    public bool TrySchedule(HostScheduledWork work)
    {
        ArgumentNullException.ThrowIfNull(work);

        if (Volatile.Read(ref _isDisposed) != 0 || !IsDeclaredWork(work))
        {
            return false;
        }

        try
        {
            _coordinatorQueue.Add(ScheduledHostWork.FireAndForget(work));
            return true;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
    }

    public bool TryRun(HostScheduledWork work, HostFrameContext frame)
    {
        ArgumentNullException.ThrowIfNull(work);

        if (Volatile.Read(ref _isDisposed) != 0 || !IsDeclaredWork(work))
        {
            return false;
        }

        if (IsOrderedWork(work.WorkId))
        {
            return false;
        }

        if (isSchedulerThread)
        {
            return false;
        }

        var scheduled = ScheduledHostWork.Blocking(work, frame);
        try
        {
            _coordinatorQueue.Add(scheduled);
        }
        catch (InvalidOperationException)
        {
            scheduled.Dispose();
            return false;
        }

        scheduled.WaitForCompletion();
        try
        {
            scheduled.ThrowIfFailed();
            return true;
        }
        finally
        {
            scheduled.Dispose();
        }
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref _isDisposed, 1) != 0)
        {
            return;
        }

        _coordinatorQueue.CompleteAdding();
        _coordinatorThread.Join();
        _workerQueue.CompleteAdding();
        foreach (var workerThread in _workerThreads)
        {
            workerThread.Join();
        }

        _coordinatorQueue.Dispose();
        _workerQueue.Dispose();
        foreach (var resourceLock in _resourceLocks.Values)
        {
            resourceLock.Dispose();
        }
    }

    private static int CreateDefaultWorkerCount()
    {
        return Math.Max(
            HostSchedulingContract.MinimumWorkerThreads,
            Environment.ProcessorCount);
    }

    private bool IsDeclaredWork(HostScheduledWork work)
    {
        return !string.IsNullOrWhiteSpace(work.WorkId) &&
            _declaredSystems.TryGetValue(work.WorkId, out var declaration) &&
            work.Phase == declaration.Phase &&
            work.Access == HostScheduledWork.AccessFromDeclaration(declaration) &&
            work.Flags == declaration.Flags;
    }

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

    private sealed class ScheduledHostWork(
        HostScheduledWork work,
        HostFrameContext frame,
        bool isBlocking)
        : IDisposable
    {
        private readonly ManualResetEventSlim _completion = new();
        private Exception? _exception;
        private int _hasCoordinatorWaiter;
        private int _isComplete;
        private int _isDisposed;

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
            try
            {
                _completion.Wait();
            }
            catch (ObjectDisposedException) when (Volatile.Read(ref _isComplete) != 0)
            {
            }
        }

        public void Fail(Exception exception)
        {
            _exception = exception;
        }

        public void Complete()
        {
            Volatile.Write(ref _isComplete, 1);
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
            if (Interlocked.Exchange(ref _isDisposed, 1) != 0)
            {
                return;
            }

            _completion.Dispose();
        }
    }

    private sealed class ResourceAccessScope : IDisposable
    {
        public static readonly ResourceAccessScope Empty = new([]);

        private readonly IReadOnlyList<LockedResource> _lockedResources;
        private bool _isDisposed;

        private ResourceAccessScope(IReadOnlyList<LockedResource> lockedResources)
        {
            _lockedResources = lockedResources;
        }

        public static ResourceAccessScope Enter(
            ConcurrentDictionary<string, ReaderWriterLockSlim> resourceLocks,
            ScheduledSystemDeclaration declaration)
        {
            var requestedResources = declaration.Reads
                .Concat(declaration.Writes)
                .GroupBy(resource => resource.ResourceId, StringComparer.Ordinal)
                .Select(group => new ScheduledResourceAccess(
                    group.Key,
                    group.Any(resource => resource.Mode == ScheduledAccessMode.Write)
                        ? ScheduledAccessMode.Write
                        : ScheduledAccessMode.Read))
                .OrderBy(resource => resource.ResourceId, StringComparer.Ordinal)
                .ToArray();
            if (requestedResources.Length == 0)
            {
                return Empty;
            }

            var lockedResources = new List<LockedResource>(requestedResources.Length);
            try
            {
                foreach (var resource in requestedResources)
                {
                    var resourceLock = resourceLocks.GetOrAdd(
                        resource.ResourceId,
                        _ => new ReaderWriterLockSlim(LockRecursionPolicy.NoRecursion));
                    if (resource.Mode == ScheduledAccessMode.Write)
                    {
                        resourceLock.EnterWriteLock();
                    }
                    else
                    {
                        resourceLock.EnterReadLock();
                    }

                    lockedResources.Add(new LockedResource(resourceLock, resource.Mode));
                }
            }
            catch
            {
                Release(lockedResources);
                throw;
            }

            return new ResourceAccessScope(lockedResources);
        }

        public void Dispose()
        {
            if (_isDisposed)
            {
                return;
            }

            _isDisposed = true;
            Release(_lockedResources);
        }

        private static void Release(IReadOnlyList<LockedResource> lockedResources)
        {
            for (var index = lockedResources.Count - 1; index >= 0; index--)
            {
                var lockedResource = lockedResources[index];
                if (lockedResource.Mode == ScheduledAccessMode.Write)
                {
                    lockedResource.ResourceLock.ExitWriteLock();
                }
                else
                {
                    lockedResource.ResourceLock.ExitReadLock();
                }
            }
        }

        private readonly record struct LockedResource(
            ReaderWriterLockSlim ResourceLock,
            ScheduledAccessMode Mode);
    }
}
