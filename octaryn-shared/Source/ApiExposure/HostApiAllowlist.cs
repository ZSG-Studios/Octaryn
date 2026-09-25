namespace Octaryn.Shared.ApiExposure;

public static class HostApiAllowlist
{
    private static readonly HashSet<string> s_allowed = new(StringComparer.Ordinal)
    {
        HostApiIds.Commands,
        HostApiIds.Frame
    };

    public static bool IsAllowed(string hostApiId)
    {
        return s_allowed.Contains(hostApiId);
    }
}
