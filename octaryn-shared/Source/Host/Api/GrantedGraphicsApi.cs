using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal sealed class GrantedGraphicsApi(IHostGraphicsApi backend, GameModuleManifest manifest) : IHostGraphicsApi, IDisposable
{
    private bool _disposed;
    private readonly bool _requested = (manifest.RequestedHostApis ?? []).Contains(HostApiIds.Graphics, StringComparer.Ordinal);
    private readonly bool _read = (manifest.Schedule?.Systems ?? []).Any(system =>
        (system.Reads ?? []).Any(resource => resource.ResourceId == HostApiIds.Graphics && resource.Mode == Host.ScheduledAccessMode.Read) ||
        (system.Writes ?? []).Any(resource => resource.ResourceId == HostApiIds.Graphics && resource.Mode == Host.ScheduledAccessMode.Write));
    private readonly bool _write = (manifest.Schedule?.Systems ?? []).Any(system => (system.Writes ?? []).Any(resource =>
        resource.ResourceId == HostApiIds.Graphics && resource.Mode == Host.ScheduledAccessMode.Write));

    public bool TryGet(out HostGraphicsSettings settings)
    {
        settings = default;
        return !_disposed && _requested && _read && backend.TryGet(out settings);
    }
    public HostGraphicsApplyResult Apply(in HostGraphicsSettings settings, bool persist) =>
        _disposed || !_requested || !_write || !HostGraphicsValidation.Valid(settings)
            ? HostGraphicsApplyResult.Rejected : backend.Apply(in settings, persist);
    public void Dispose() => _disposed = true;
}
