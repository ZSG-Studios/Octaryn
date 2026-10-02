namespace Octaryn.Shared.Host;

using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host.Api;

internal static class HostModuleContext
{
    public static ModuleHostContext Create(
        GameModuleManifest manifest,
        IHostCommandSink commands,
        IHostApiProvider? apis = null,
        string? moduleContentRoot = null,
        NativeScheduleRuntime? sceneScheduler = null)
    {
        var requestedHostApis = manifest.RequestedHostApis ?? [];
        var schedule = manifest.Schedule ?? new GameModuleScheduleDeclaration([]);
        var grantsCommandSink = requestedHostApis.Contains(HostApiIds.Commands, StringComparer.Ordinal) &&
            HasScheduledWrite(schedule.Systems ?? [], HostApiIds.Commands);
        var root = moduleContentRoot ?? GameModuleBundle.ResolveRoot(AppContext.BaseDirectory);
        var context = new ModuleHostContext(
            grantsCommandSink
                ? commands
                : DeniedHostCommandSink.Instance,
            IsRequested(requestedHostApis, HostApiIds.Time) ? apis?.GetTimeApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Diagnostics) ? apis?.GetDiagnosticsApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Physics) ? apis?.GetPhysicsApi() : null,
            IsRequested(requestedHostApis, HostApiIds.World) ? apis?.GetWorldApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Player) ? apis?.GetPlayerApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Ecs) ? apis?.GetEcsApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Input) ? apis?.GetInputApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Scheduling) ? apis?.GetSchedulingApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Audio) ? apis?.GetAudioApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Ui) ? apis?.GetUiApi(manifest, root) : null,
            IsRequested(requestedHostApis, HostApiIds.Replication) ? apis?.GetReplicationApi() : null,
            IsRequested(requestedHostApis, HostApiIds.Content)
                ? apis?.GetContentApi(manifest, root) : null);
        return context with
        {
            ScenePhysics = IsRequested(requestedHostApis,HostApiIds.ScenePhysics) ? apis?.GetScenePhysicsApi() : null,
            Residency = IsRequested(requestedHostApis,HostApiIds.Residency) && apis?.GetResidencyApi() is { } residency
                ? new GrantedResidencyApi(residency,manifest) : null,
            Transition = IsRequested(requestedHostApis,HostApiIds.Transition) ? apis?.GetTransitionApi(manifest,root) : null,
            Application = IsRequested(requestedHostApis, HostApiIds.Application) && apis?.GetApplicationApi() is { } application
                ? new GrantedApplicationApi(application, manifest) : null,
            Graphics = IsRequested(requestedHostApis, HostApiIds.Graphics) && apis?.GetGraphicsApi() is { } graphics
                ? new GrantedGraphicsApi(graphics, manifest) : null,
            Scene = IsRequested(requestedHostApis, HostApiIds.Scene)
                ? apis?.GetSceneApi(manifest, root) ?? NativeScenePreparationApi.TryCreate(manifest, root, sceneScheduler)
                : null
        };
    }

    private static bool IsRequested(IReadOnlyList<string> requestedHostApis, string hostApiId)
    {
        return requestedHostApis.Contains(hostApiId, StringComparer.Ordinal);
    }

    private static bool HasScheduledWrite(
        IReadOnlyList<ScheduledSystemDeclaration> systems,
        string resourceId)
    {
        return systems.Any(system => (system.Writes ?? [])
            .Any(resource => resource.ResourceId == resourceId && resource.Mode == ScheduledAccessMode.Write));
    }

    private sealed class DeniedHostCommandSink : IHostCommandSink
    {
        public static readonly DeniedHostCommandSink Instance = new();

        private DeniedHostCommandSink()
        {
        }

        public bool Enqueue(HostCommand command)
        {
            _ = command;
            return false;
        }
    }
}
