namespace Octaryn.Shared.ApiExposure;

public static class HostApiAllowlist
{
    private static readonly HashSet<string> s_allowed = new(StringComparer.Ordinal)
    {
        HostApiIds.Commands,
        HostApiIds.Frame,
        HostApiIds.Time,
        HostApiIds.Diagnostics,
        HostApiIds.Physics,
        HostApiIds.World,
        HostApiIds.Input,
        HostApiIds.Scheduling,
        HostApiIds.Audio,
        HostApiIds.Ui,
        HostApiIds.Replication,
        HostApiIds.Player,
        HostApiIds.Ecs
    };

    public static bool IsAllowed(string hostApiId)
    {
        return s_allowed.Contains(hostApiId);
    }
}
