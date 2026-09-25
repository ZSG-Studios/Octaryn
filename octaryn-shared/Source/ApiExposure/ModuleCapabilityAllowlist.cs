namespace Octaryn.Shared.ApiExposure;

public static class ModuleCapabilityAllowlist
{
    private static readonly HashSet<string> s_allowed = new(StringComparer.Ordinal)
    {
        ModuleCapabilityIds.ContentItems,
        ModuleCapabilityIds.GameplayInteractions,
        ModuleCapabilityIds.GameplayRules
    };

    public static bool IsAllowed(string capabilityId)
    {
        return s_allowed.Contains(capabilityId);
    }
}
