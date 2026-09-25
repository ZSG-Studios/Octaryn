namespace Octaryn.Shared.FrameworkAllowlist;

public static class ModuleBuildPackageAllowlist
{
    private static readonly HashSet<string> s_allowed = new(StringComparer.Ordinal)
    {
        AllowedPackageIds.ArchSystemSourceGenerator
    };

    public static bool IsAllowed(string packageId)
    {
        return s_allowed.Contains(packageId);
    }
}
