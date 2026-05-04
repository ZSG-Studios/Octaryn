namespace Octaryn.Shared.Host;

using Octaryn.Shared.ApiExposure;

internal sealed record HostScheduledWork(
    string WorkId,
    HostWorkPhase Phase,
    HostWorkAccess Access,
    HostWorkScheduleFlags Flags,
    Action<HostScheduledWorkContext> Execute)
{
    public static HostScheduledWork FromDeclaration(
        ScheduledSystemDeclaration declaration,
        Action<HostScheduledWorkContext> execute)
    {
        return new HostScheduledWork(
            declaration.SystemId,
            declaration.Phase,
            AccessFromDeclaration(declaration),
            declaration.Flags,
            execute);
    }

    public static HostWorkAccess AccessFromDeclaration(ScheduledSystemDeclaration declaration)
    {
        var access = HostWorkAccess.None;
        foreach (var read in declaration.Reads)
        {
            access |= AccessForResource(read.ResourceId, isWrite: false);
        }

        foreach (var write in declaration.Writes)
        {
            access |= AccessForResource(write.ResourceId, isWrite: true);
        }

        return access;
    }

    private static HostWorkAccess AccessForResource(string resourceId, bool isWrite)
    {
        if (resourceId == HostApiIds.Frame)
        {
            return HostWorkAccess.InputSnapshot | HostWorkAccess.FrameTimingSnapshot;
        }

        if (resourceId == HostApiIds.Commands)
        {
            return isWrite ? HostWorkAccess.CommandSinkWrite : HostWorkAccess.None;
        }

        if (resourceId.EndsWith(".content", StringComparison.Ordinal) ||
            resourceId.Contains(".content.", StringComparison.Ordinal))
        {
            return isWrite ? HostWorkAccess.None : HostWorkAccess.ContentRegistryRead;
        }

        return isWrite ? HostWorkAccess.GameplayStateWrite : HostWorkAccess.GameplayStateRead;
    }
}
