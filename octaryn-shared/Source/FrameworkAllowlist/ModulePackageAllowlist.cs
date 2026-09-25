namespace Octaryn.Shared.FrameworkAllowlist;

public static class ModulePackageAllowlist
{
    private static readonly HashSet<string> s_allowed = new(StringComparer.Ordinal)
    {
        AllowedPackageIds.Arch,
        AllowedPackageIds.ArchSystem,
        AllowedPackageIds.ArchEventBus,
        AllowedPackageIds.ArchRelationships
    };

    public static bool IsAllowed(string packageId)
    {
        return s_allowed.Contains(packageId);
    }
}
