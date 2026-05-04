using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.Host;
using Octaryn.Shared.ModuleSandbox;

namespace Octaryn.Shared.GameModules;

public static partial class GameModuleValidator
{
    public static ModuleValidationReport Validate(GameModuleManifest? manifest)
    {
        var report = new ModuleValidationReport();
        if (manifest is null)
        {
            report.AddError("module.manifest.required", "Module manifest is required.");
            return report;
        }

        var compatibility = manifest.Compatibility;
        if (compatibility is null)
        {
            report.AddError("module.compatibility.required", "Module compatibility declaration is required.");
            compatibility = new GameModuleCompatibility(string.Empty, string.Empty, string.Empty, SupportsMultiplayer: false);
        }

        var requiredCapabilities = manifest.RequiredCapabilities ?? [];
        var requestedHostApis = manifest.RequestedHostApis ?? [];
        var requestedRuntimePackages = manifest.RequestedRuntimePackages ?? [];
        var requestedBuildPackages = manifest.RequestedBuildPackages ?? [];
        var requestedFrameworkApiGroups = manifest.RequestedFrameworkApiGroups ?? [];
        var moduleDependencies = manifest.ModuleDependencies ?? [];
        var contentDeclarations = manifest.ContentDeclarations ?? [];
        var assetDeclarations = manifest.AssetDeclarations ?? [];
        var schedule = manifest.Schedule ?? new GameModuleScheduleDeclaration([]);
        var scheduledSystems = schedule.Systems ?? [];

        RequireText(report, manifest.ModuleId, "module.id.required", "Module ID is required.");
        RequireText(report, manifest.DisplayName, "module.display_name.required", "Display name is required.");
        RequireText(report, manifest.Version, "module.version.required", "Module version is required.");
        RequireText(report, manifest.OctarynApiVersion, "module.api_version.required", "Octaryn API version is required.");
        RequireText(report, compatibility.MinimumHostApiVersion, "module.compatibility.minimum_host_api.required", "Minimum host API version is required.");
        RequireText(report, compatibility.MaximumHostApiVersion, "module.compatibility.maximum_host_api.required", "Maximum host API version is required.");
        RequireText(report, compatibility.SaveCompatibilityId, "module.compatibility.save_id.required", "Save compatibility ID is required.");
        RequireVersion(report, manifest.Version, "module.version.invalid", "Module version is invalid.");
        RequireVersion(report, manifest.OctarynApiVersion, "module.api_version.invalid", "Octaryn API version is invalid.");
        RequireVersion(report, compatibility.MinimumHostApiVersion, "module.compatibility.minimum_host_api.invalid", "Minimum host API version is invalid.");
        RequireVersion(report, compatibility.MaximumHostApiVersion, "module.compatibility.maximum_host_api.invalid", "Maximum host API version is invalid.");
        RequireVersionRange(
            report,
            compatibility.MinimumHostApiVersion,
            compatibility.MaximumHostApiVersion,
            "module.compatibility.host_api_range.invalid",
            "Host API compatibility range is invalid.");
        RequireUnique(report, requiredCapabilities, "module.capability.duplicate", "Required capability is duplicated.");
        RequireUnique(report, requestedHostApis, "module.host_api.duplicate", "Requested host API is duplicated.");
        RequireUnique(report, requestedRuntimePackages, "module.runtime_package.duplicate", "Requested runtime package is duplicated.");
        RequireUnique(report, requestedBuildPackages, "module.build_package.duplicate", "Requested build package is duplicated.");
        RequireUnique(report, requestedFrameworkApiGroups, "module.framework_api.duplicate", "Requested framework API group is duplicated.");
        RequireUnique(
            report,
            scheduledSystems.Select(system => system.SystemId),
            "module.schedule.system.duplicate",
            "Scheduled system is duplicated.");
        RequireUnique(
            report,
            moduleDependencies.Select(dependency => dependency.ModuleId),
            "module.dependency.duplicate",
            "Module dependency is duplicated.");
        RequireUnique(
            report,
            contentDeclarations.Select(content => content.ContentId),
            "module.content.duplicate",
            "Content declaration is duplicated.");
        RequireUnique(
            report,
            contentDeclarations.Select(content => content.RelativePath),
            "module.content.path.duplicate",
            "Content declaration path is duplicated.");
        RequireUnique(
            report,
            assetDeclarations.Select(asset => asset.AssetId),
            "module.asset.duplicate",
            "Asset declaration is duplicated.");
        RequireUnique(
            report,
            assetDeclarations.Select(asset => asset.RelativePath),
            "module.asset.path.duplicate",
            "Asset declaration path is duplicated.");
        RequireUnique(
            report,
            contentDeclarations.Select(content => content.ContentId)
                .Concat(assetDeclarations.Select(asset => asset.AssetId))
                .Concat(scheduledSystems.Select(system => system.SystemId)),
            "module.declaration.id.duplicate",
            "Module declaration ID is duplicated across content, assets, and scheduled systems.");
        RequireModuleOwnedDeclarations(report, manifest.ModuleId, contentDeclarations, assetDeclarations, scheduledSystems);
        RequireDeclarations(report, moduleDependencies);
        RequireDeclarations(report, contentDeclarations);
        RequireDeclarations(report, assetDeclarations);
        RequireDeclarations(report, scheduledSystems);
        RequireScheduleGraph(report, scheduledSystems);
        RequireScheduleResourceConflicts(report, scheduledSystems);
        RequireAllowed(
            report,
            requiredCapabilities,
            ModuleCapabilityAllowlist.IsAllowed,
            "module.capability.not_allowed",
            "Required capability is not exposed by the host API.");
        RequireAllowed(
            report,
            requestedHostApis,
            HostApiAllowlist.IsAllowed,
            "module.host_api.not_allowed",
            "Requested host API is not exposed.");
        RequireAllowed(
            report,
            requestedRuntimePackages,
            ModulePackageAllowlist.IsAllowed,
            "module.runtime_package.not_allowed",
            "Requested runtime package is not allowed for game modules.");
        RequireAllowed(
            report,
            requestedBuildPackages,
            ModuleBuildPackageAllowlist.IsAllowed,
            "module.build_package.not_allowed",
            "Requested build package is not allowed for game modules.");
        RequireAllowed(
            report,
            requestedFrameworkApiGroups,
            FrameworkApiGroupAllowlist.IsAllowed,
            "module.framework_api.not_allowed",
            "Requested framework API group is not allowed for game modules.");
        RequireDenied(
            report,
            requestedFrameworkApiGroups,
            DeniedFrameworkApiGroups.Values,
            "module.framework_api.denied",
            "Requested framework API group is explicitly denied for game modules.");
        if (HasScheduledRead(scheduledSystems, HostApiIds.Frame) &&
            !requestedHostApis.Contains(HostApiIds.Frame, StringComparer.Ordinal))
        {
            report.AddError(
                "module.schedule.frame.read.required",
                $"Scheduled frame reads require host API: {HostApiIds.Frame}");
        }

        if (requestedHostApis.Contains(HostApiIds.Commands, StringComparer.Ordinal) &&
            !HasScheduledWrite(scheduledSystems, HostApiIds.Commands))
        {
            report.AddError(
                "module.schedule.commands.write.required",
                $"Requested command access must be declared as a scheduled write resource: {HostApiIds.Commands}");
        }

        if (HasScheduledWrite(scheduledSystems, HostApiIds.Commands) &&
            !requestedHostApis.Contains(HostApiIds.Commands, StringComparer.Ordinal))
        {
            report.AddError(
                "module.schedule.commands.host_api.required",
                $"Scheduled command writes require host API: {HostApiIds.Commands}");
        }

        if ((requestedHostApis.Contains(HostApiIds.Commands, StringComparer.Ordinal) ||
             HasScheduledWrite(scheduledSystems, HostApiIds.Commands)) &&
            !requiredCapabilities.Contains(ModuleCapabilityIds.WorldBlockEdits, StringComparer.Ordinal))
        {
            report.AddError(
                "module.capability.world_block_edits.required",
                $"Block edit command access requires capability: {ModuleCapabilityIds.WorldBlockEdits}");
        }

        return report;
    }
}
