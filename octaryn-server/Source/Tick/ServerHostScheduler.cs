using System.Collections.Concurrent;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Tick;

internal sealed partial class ServerHostScheduler : IHostScheduler, IDisposable
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

    public ServerHostScheduler()
        : this(CreateDefaultWorkerCount())
    {
    }

    internal ServerHostScheduler(int workerThreadCapacity)
        : this(workerThreadCapacity, [])
    {
    }

    internal ServerHostScheduler(IReadOnlyList<ScheduledSystemDeclaration> declaredSystems)
        : this(CreateDefaultWorkerCount(), declaredSystems)
    {
    }

    internal ServerHostScheduler(
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
            Name = "octaryn.server.coordinator"
        };
        _workerThreads = new Thread[WorkerThreadCapacity];
        for (var index = 0; index < _workerThreads.Length; index++)
        {
            var workerIndex = index;
            _workerThreads[index] = new Thread(() => RunWorker(workerIndex))
            {
                IsBackground = true,
                Name = $"octaryn.server.worker.{workerIndex}"
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
}
