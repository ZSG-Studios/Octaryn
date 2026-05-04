using Octaryn.Server.Modules.Bundled;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static class ServerModuleValidation
{
    private static readonly HashSet<string> s_clientOnlyHostApis = new(StringComparer.Ordinal)
    {
        HostApiIds.ClientCommands
    };

    public static ModuleValidationReport ValidateBundledGameModule()
    {
        var registration = Loader.LoadBundledRegistration();
        return Validate(registration);
    }

    public static ModuleValidationReport Validate(IGameModuleRegistration registration)
    {
        var report = GameModuleValidator.Validate(registration.Manifest);
        ValidateServerCompatibility(report, registration.Manifest);
        return report;
    }

    private static void ValidateServerCompatibility(ModuleValidationReport report, GameModuleManifest manifest)
    {
        RequireCapability(report, manifest, ModuleCapabilityIds.GameplayRules);
        RequireHostApi(report, manifest, HostApiIds.Commands);
        RejectHostApis(report, manifest, s_clientOnlyHostApis, "server.module.host_api.client_only");
        RejectHostApi(report, manifest, HostApiIds.Replication, "server.module.host_api.replication_not_supported");

        if (manifest.AssetDeclarations.Any(asset => asset.AssetKind == "shader" || asset.AssetKind == "ui"))
        {
            report.AddError(
                "server.module.presentation_asset.invalid",
                "Server modules cannot require shader or UI asset declarations for authority validation.");
        }

        foreach (var system in manifest.Schedule.Systems)
        {
            if (system.Phase is HostWorkPhase.PresentationPrepare or HostWorkPhase.AssetProcessing)
            {
                report.AddError(
                    "server.module.schedule.phase.invalid",
                    $"Server modules cannot declare presentation phases. System: {system.SystemId}");
            }
        }

        if (!manifest.Compatibility.SaveCompatibilityId.StartsWith($"{manifest.ModuleId}.save.", StringComparison.Ordinal))
        {
            report.AddError(
                "server.module.save_compatibility_id.invalid",
                "Save compatibility ID must be module-owned and stable.");
        }

        if (manifest.Compatibility.SupportsMultiplayer)
        {
            report.AddError(
                "server.module.multiplayer.not_supported",
                "Server multiplayer policy is deny-by-default until replication contracts are implemented.");
        }
    }

    private static void RequireCapability(ModuleValidationReport report, GameModuleManifest manifest, string capability)
    {
        if (!manifest.RequiredCapabilities.Contains(capability, StringComparer.Ordinal))
        {
            report.AddError(
                "server.module.capability.required",
                $"Server module validation requires capability {capability}.");
        }
    }

    private static void RequireHostApi(ModuleValidationReport report, GameModuleManifest manifest, string hostApi)
    {
        if (!manifest.RequestedHostApis.Contains(hostApi, StringComparer.Ordinal))
        {
            report.AddError(
                "server.module.host_api.required",
                $"Server module validation requires host API {hostApi}.");
        }
    }

    private static void RejectHostApis(
        ModuleValidationReport report,
        GameModuleManifest manifest,
        IReadOnlySet<string> hostApis,
        string code)
    {
        foreach (var hostApi in manifest.RequestedHostApis)
        {
            if (hostApis.Contains(hostApi))
            {
                report.AddError(code, $"Server module requested unsupported host API {hostApi}.");
            }
        }
    }

    private static void RejectHostApi(
        ModuleValidationReport report,
        GameModuleManifest manifest,
        string hostApi,
        string code)
    {
        if (manifest.RequestedHostApis.Contains(hostApi, StringComparer.Ordinal))
        {
            report.AddError(code, $"Server module requested unsupported host API {hostApi}.");
        }
    }
}
