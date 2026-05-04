namespace Octaryn.Shared.Host;

internal readonly record struct HostScheduledWorkContext(
    HostFrameContext Frame,
    HostThreadRole ThreadRole,
    int WorkerIndex,
    bool IsCancellationRequested);
