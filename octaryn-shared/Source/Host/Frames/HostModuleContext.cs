namespace Octaryn.Shared.Host;

using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;

internal static class HostModuleContext
{
    public static ModuleHostContext Create(
        GameModuleManifest manifest,
        IHostCommandSink commands)
    {
        var requestedHostApis = manifest.RequestedHostApis ?? [];
        var schedule = manifest.Schedule ?? new GameModuleScheduleDeclaration([]);
        var grantsCommandSink = requestedHostApis.Contains(HostApiIds.Commands, StringComparer.Ordinal) &&
            HasScheduledWrite(schedule.Systems ?? [], HostApiIds.Commands);
        return new ModuleHostContext(
            grantsCommandSink
                ? commands
                : DeniedHostCommandSink.Instance);
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
