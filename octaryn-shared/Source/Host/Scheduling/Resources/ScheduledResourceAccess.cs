namespace Octaryn.Shared.Host;

public readonly record struct ScheduledResourceAccess(
    string ResourceId,
    ScheduledAccessMode Mode);
