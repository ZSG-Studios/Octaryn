using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;

namespace Octaryn.Client.Validation;

internal static class ModuleValidation
{
    private static readonly HashSet<string> s_serverOnlyHostApis = new(StringComparer.Ordinal)
    {
        HostApiIds.ServerSnapshots
    };

    public static ModuleValidationReport Validate(IGameModuleRegistration registration)
    {
        var report = GameModuleValidator.Validate(registration.Manifest);
        ValidateClientCompatibility(report, registration.Manifest);
        return report;
    }

    private static void ValidateClientCompatibility(ModuleValidationReport report, GameModuleManifest manifest)
    {
        RequireHostApi(report, manifest, HostApiIds.Frame);
        RejectHostApis(report, manifest, s_serverOnlyHostApis, "client.module.host_api.server_only");
        RejectHostApi(report, manifest, HostApiIds.Replication, "client.module.host_api.replication_not_supported");

        foreach (var asset in manifest.AssetDeclarations)
        {
            if (asset.AssetKind == "shader" && !asset.RelativePath.StartsWith("Shaders/", StringComparison.Ordinal))
            {
                report.AddError(
                    "client.module.shader_asset.path.invalid",
                    $"Shader assets must be declared under Shaders/. Asset: {asset.AssetId}");
            }
        }

        foreach (var system in manifest.Schedule.Systems)
        {
            if (system.Phase is HostWorkPhase.PersistencePrepare or HostWorkPhase.Replication)
            {
                report.AddError(
                    "client.module.schedule.phase.invalid",
                    $"Client modules cannot declare server authority phases. System: {system.SystemId}");
            }
        }

        if (manifest.Compatibility.SupportsMultiplayer)
        {
            report.AddError(
                "client.module.multiplayer.not_supported",
                "Client multiplayer policy is deny-by-default until replication contracts are implemented.");
        }
    }

    private static void RequireHostApi(ModuleValidationReport report, GameModuleManifest manifest, string hostApi)
    {
        if (!manifest.RequestedHostApis.Contains(hostApi, StringComparer.Ordinal))
        {
            report.AddError(
                "client.module.host_api.required",
                $"Client module activation requires host API {hostApi}.");
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
                report.AddError(code, $"Client module requested unsupported host API {hostApi}.");
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
            report.AddError(code, $"Client module requested unsupported host API {hostApi}.");
        }
    }
}
