namespace Octaryn.Shared.Host;

internal sealed record HostSchedulerDiagnostics(
    string CoordinatorThreadName,
    bool IsCoordinatorThreadAlive,
    IReadOnlyList<string> WorkerThreadNames,
    int LiveWorkerThreadCount,
    long FireAndForgetFailureCount,
    string? LastFireAndForgetFailureWorkId,
    string? LastFireAndForgetFailureType);
