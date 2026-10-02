using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal sealed class GrantedApplicationApi(IHostApplicationApi backend, GameModuleManifest manifest) : IHostApplicationApi, IDisposable
{
    private bool _disposed;
    private readonly bool _write = (manifest.RequestedHostApis ?? []).Contains(HostApiIds.Application, StringComparer.Ordinal) &&
        (manifest.Schedule?.Systems ?? []).Any(system => (system.Writes ?? []).Any(resource =>
            resource.ResourceId == HostApiIds.Application && resource.Mode == Host.ScheduledAccessMode.Write));

    public bool RequestExit() => !_disposed && _write && backend.RequestExit();
    public void Dispose() => _disposed = true;
}
