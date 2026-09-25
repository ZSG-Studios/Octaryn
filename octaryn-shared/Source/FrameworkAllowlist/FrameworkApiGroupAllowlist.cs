namespace Octaryn.Shared.FrameworkAllowlist;

public static class FrameworkApiGroupAllowlist
{
    public static readonly IReadOnlyList<string> Values =
    [
        FrameworkApiGroupIds.BclPrimitives,
        FrameworkApiGroupIds.BclCollections,
        FrameworkApiGroupIds.BclMemory,
        FrameworkApiGroupIds.BclMath,
        FrameworkApiGroupIds.BclTime,
        FrameworkApiGroupIds.BclText
    ];

    private static readonly HashSet<string> s_allowed = new(Values, StringComparer.Ordinal);

    public static bool IsAllowed(string frameworkApiGroupId)
    {
        return s_allowed.Contains(frameworkApiGroupId);
    }
}
