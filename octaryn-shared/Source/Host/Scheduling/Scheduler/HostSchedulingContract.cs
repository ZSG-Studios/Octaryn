namespace Octaryn.Shared.Host;

internal static class HostSchedulingContract
{
    public const int MinimumWorkerThreads = 2;

    public const string MainThreadRole = "host.thread.main";
    public const string CoordinatorThreadRole = "host.thread.coordinator";
    public const string WorkerPoolRole = "host.thread.worker_pool";

    public static bool IsValidWorkerThreadCapacity(int workerThreadCapacity)
    {
        return workerThreadCapacity >= MinimumWorkerThreads;
    }
}
