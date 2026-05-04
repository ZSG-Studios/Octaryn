namespace Octaryn.Shared.Host;

public sealed record ScheduledSystemDeclaration(
    string SystemId,
    HostWorkPhase Phase,
    string FrameOrTickOwner,
    IReadOnlyList<ScheduledResourceAccess> Reads,
    IReadOnlyList<ScheduledResourceAccess> Writes,
    IReadOnlyList<string> RunsAfter,
    IReadOnlyList<string> RunsBefore,
    HostWorkScheduleFlags Flags,
    string CommitBarrier);
