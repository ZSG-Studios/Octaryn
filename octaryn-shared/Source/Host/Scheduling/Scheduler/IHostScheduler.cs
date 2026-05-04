namespace Octaryn.Shared.Host;

internal interface IHostScheduler
{
    int MinimumWorkerThreads { get; }

    int WorkerThreadCapacity { get; }

    bool IsWorkerPoolAvailable { get; }

    bool TrySchedule(HostScheduledWork work);

    bool TryRun(HostScheduledWork work, HostFrameContext frame);
}
