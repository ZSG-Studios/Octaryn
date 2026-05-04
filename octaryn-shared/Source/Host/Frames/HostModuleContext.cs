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
        var requiredCapabilities = manifest.RequiredCapabilities ?? [];
        var schedule = manifest.Schedule ?? new GameModuleScheduleDeclaration([]);
        var grantsCommandSink = requestedHostApis.Contains(HostApiIds.Commands, StringComparer.Ordinal) &&
            requiredCapabilities.Contains(ModuleCapabilityIds.WorldBlockEdits, StringComparer.Ordinal) &&
            HasScheduledWrite(schedule.Systems ?? [], HostApiIds.Commands);
        return new ModuleHostContext(
            grantsCommandSink
                ? new ScopedModuleCommandRequests(commands)
                : DeniedModuleCommandRequests.Instance);
    }

    private static bool HasScheduledWrite(
        IReadOnlyList<ScheduledSystemDeclaration> systems,
        string resourceId)
    {
        return systems.Any(system => (system.Writes ?? [])
            .Any(resource => resource.ResourceId == resourceId && resource.Mode == ScheduledAccessMode.Write));
    }

    private sealed class DeniedModuleCommandRequests : IModuleCommandRequests
    {
        public static readonly DeniedModuleCommandRequests Instance = new();

        private DeniedModuleCommandRequests()
        {
        }

        public bool TryRequest(ModuleCommandRequest request)
        {
            _ = request;
            return false;
        }
    }

    private sealed class ScopedModuleCommandRequests(IHostCommandSink inner) : IModuleCommandRequests
    {
        public bool TryRequest(ModuleCommandRequest request)
        {
            if (!HostCommandWriteScope.IsActive)
            {
                return false;
            }

            return request.Kind switch
            {
                ModuleCommandRequestKind.SetBlock => EnqueueSetBlock(request),
                _ => false
            };
        }

        private bool EnqueueSetBlock(ModuleCommandRequest request)
        {
            return inner.Enqueue(new HostCommand
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock,
                Flags = HostCommand.CriticalFlag,
                RequestId = request.RequestId,
                A = request.BlockEdit.Position.X,
                B = request.BlockEdit.Position.Y,
                C = request.BlockEdit.Position.Z,
                D = request.BlockEdit.Block.Value
            });
        }
    }
}
