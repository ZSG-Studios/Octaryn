using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
namespace Octaryn.Shared.Host.Api;
internal sealed class GrantedResidencyApi(IHostResidencyApi backend,GameModuleManifest manifest) : IHostResidencyApi,IDisposable
{
    private bool _disposed;
    private readonly bool _requested=(manifest.RequestedHostApis ?? []).Contains(HostApiIds.Residency,StringComparer.Ordinal);
    private readonly bool _read=(manifest.Schedule?.Systems ?? []).Any(system =>
        (system.Reads ?? []).Any(resource => resource.ResourceId==HostApiIds.Residency && resource.Mode==Host.ScheduledAccessMode.Read) ||
        (system.Writes ?? []).Any(resource => resource.ResourceId==HostApiIds.Residency && resource.Mode==Host.ScheduledAccessMode.Write));
    private readonly bool _write=(manifest.Schedule?.Systems ?? []).Any(system =>
        (system.Writes ?? []).Any(resource => resource.ResourceId==HostApiIds.Residency && resource.Mode==Host.ScheduledAccessMode.Write));
    public bool TryGetActorPosition(out HostRegionAnchor position)
    {
        position=default;return !_disposed && _requested && _read && backend.TryGetActorPosition(out position);
    }
    public bool TryGetCount(out uint count,out ulong generation)
    {
        count=0;generation=0;return !_disposed && _requested && _read && backend.TryGetCount(out count,out generation);
    }
    public bool TryGetRegion(uint index,out HostRegionResidency region)
    {
        region=default;return !_disposed && _requested && _read && backend.TryGetRegion(index,out region);
    }
    public bool SetDesiredRegions(ReadOnlySpan<uint> wanted,ReadOnlySpan<uint> retained) =>
        !_disposed && _requested && _write && backend.SetDesiredRegions(wanted,retained);
    public void Dispose() => _disposed=true;
}
