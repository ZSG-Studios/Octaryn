using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.Host;

namespace Octaryn.Shared.GameModules;

public static partial class GameModuleValidator
{
    private const HostWorkScheduleFlags AllowedScheduleFlags =
        HostWorkScheduleFlags.DeterministicOrder |
        HostWorkScheduleFlags.CanRunInParallel |
        HostWorkScheduleFlags.RequiresTickBarrier;

    private static readonly HashSet<string> s_hostNeutralFrameOrTickOwners = new(StringComparer.Ordinal)
    {
        HostScheduleIds.FrameOrTickOwner
    };

    private static readonly HashSet<string> s_hostNeutralCommitBarriers = new(StringComparer.Ordinal)
    {
        HostScheduleIds.FrameOrTickEndBarrier
    };

    private static readonly HashSet<string> s_allowedHostReadResources = new(StringComparer.Ordinal)
    {
        HostApiIds.Frame
    };

    private static readonly HashSet<string> s_allowedHostWriteResources = new(StringComparer.Ordinal)
    {
        HostApiIds.Commands
    };

    private static readonly HashSet<string> s_contentKinds = new(StringComparer.Ordinal)
    {
        "block",
        "item",
        "material",
        "recipe",
        "tag",
        "loot_table",
        "feature",
        "biome",
        "rule"
    };

    private static readonly HashSet<string> s_assetKinds = new(StringComparer.Ordinal)
    {
        "atlas",
        "blockstate",
        "model",
        "shader",
        "texture",
        "ui",
        "audio"
    };

    private static void RequireDeclarations(
        ModuleValidationReport report,
        IReadOnlyList<GameModuleDependency> dependencies)
    {
        foreach (var dependency in dependencies)
        {
            RequireText(report, dependency.ModuleId, "module.dependency.id.required", "Module dependency ID is required.");
            RequireText(report, dependency.MinimumVersion, "module.dependency.minimum_version.required", "Module dependency minimum version is required.");
            RequireText(report, dependency.MaximumVersion, "module.dependency.maximum_version.required", "Module dependency maximum version is required.");
            RequireVersion(report, dependency.MinimumVersion, "module.dependency.minimum_version.invalid", "Module dependency minimum version is invalid.");
            RequireVersion(report, dependency.MaximumVersion, "module.dependency.maximum_version.invalid", "Module dependency maximum version is invalid.");
            RequireVersionRange(
                report,
                dependency.MinimumVersion,
                dependency.MaximumVersion,
                "module.dependency.version_range.invalid",
                "Module dependency version range is invalid.");
        }
    }

    private static void RequireDeclarations(
        ModuleValidationReport report,
        IReadOnlyList<GameModuleContentDeclaration> contents)
    {
        foreach (var content in contents)
        {
            RequireText(report, content.ContentId, "module.content.id.required", "Content ID is required.");
            RequireText(report, content.ContentKind, "module.content.kind.required", "Content kind is required.");
            RequireText(report, content.RelativePath, "module.content.path.required", "Content path is required.");
            RequireVocabulary(report, content.ContentKind, s_contentKinds, "module.content.kind.invalid", "Content kind is not recognized.");
            RequireSafeRelativePath(report, content.RelativePath, "module.content.path.invalid", "Content path must be a safe relative path.");
            if (!string.IsNullOrWhiteSpace(content.RelativePath) &&
                !content.RelativePath.StartsWith("Data/", StringComparison.Ordinal))
            {
                report.AddError(
                    "module.content.path.owner.invalid",
                    $"Content declarations must live under Data/. Value: {content.RelativePath}");
            }
        }
    }

    private static void RequireDeclarations(
        ModuleValidationReport report,
        IReadOnlyList<GameModuleAssetDeclaration> assets)
    {
        foreach (var asset in assets)
        {
            RequireText(report, asset.AssetId, "module.asset.id.required", "Asset ID is required.");
            RequireText(report, asset.AssetKind, "module.asset.kind.required", "Asset kind is required.");
            RequireText(report, asset.RelativePath, "module.asset.path.required", "Asset path is required.");
            RequireVocabulary(report, asset.AssetKind, s_assetKinds, "module.asset.kind.invalid", "Asset kind is not recognized.");
            RequireSafeRelativePath(report, asset.RelativePath, "module.asset.path.invalid", "Asset path must be a safe relative path.");
            if (!string.IsNullOrWhiteSpace(asset.RelativePath) &&
                !asset.RelativePath.StartsWith("Assets/", StringComparison.Ordinal) &&
                !asset.RelativePath.StartsWith("Shaders/", StringComparison.Ordinal))
            {
                report.AddError(
                    "module.asset.path.owner.invalid",
                    $"Asset declarations must live under Assets/ or Shaders/. Value: {asset.RelativePath}");
            }
        }
    }

    private static void RequireDeclarations(
        ModuleValidationReport report,
        IReadOnlyList<ScheduledSystemDeclaration> systems)
    {
        foreach (var system in systems)
        {
            RequireText(report, system.SystemId, "module.schedule.system_id.required", "Scheduled system ID is required.");
            RequireText(report, system.FrameOrTickOwner, "module.schedule.owner.required", "Scheduled system frame/tick owner is required.");
            RequireText(report, system.CommitBarrier, "module.schedule.commit_barrier.required", "Scheduled system commit barrier is required.");
            RequireVocabulary(
                report,
                system.FrameOrTickOwner,
                s_hostNeutralFrameOrTickOwners,
                "module.schedule.owner.invalid",
                "Scheduled system owner must be host-neutral.");
            RequireVocabulary(
                report,
                system.CommitBarrier,
                s_hostNeutralCommitBarriers,
                "module.schedule.commit_barrier.invalid",
                "Scheduled system commit barrier must be host-neutral.");
            if (!Enum.IsDefined(system.Phase))
            {
                report.AddError("module.schedule.phase.invalid", $"Scheduled system phase is invalid. System: {system.SystemId}");
            }

            if ((system.Flags & ~AllowedScheduleFlags) != 0)
            {
                report.AddError("module.schedule.flags.invalid", $"Scheduled system flags are invalid. System: {system.SystemId}");
            }

            RequireResourceAccess(report, system.SystemId, system.Reads ?? [], "read");
            RequireResourceAccess(report, system.SystemId, system.Writes ?? [], "write");
            RequireUnique(
                report,
                (system.Reads ?? []).Select(resource => resource.ResourceId),
                "module.schedule.read.resource_duplicate",
                $"Scheduled system has duplicated read resource. System: {system.SystemId}");
            RequireUnique(
                report,
                (system.Writes ?? []).Select(resource => resource.ResourceId),
                "module.schedule.write.resource_duplicate",
                $"Scheduled system has duplicated write resource. System: {system.SystemId}");
            RequireNoResourceOverlap(report, system);
            RequireUnique(
                report,
                system.RunsAfter ?? [],
                "module.schedule.runs_after.duplicate",
                $"Scheduled system has duplicated RunsAfter dependency. System: {system.SystemId}");
            RequireUnique(
                report,
                system.RunsBefore ?? [],
                "module.schedule.runs_before.duplicate",
                $"Scheduled system has duplicated RunsBefore dependency. System: {system.SystemId}");
        }
    }

    private static void RequireModuleOwnedDeclarations(
        ModuleValidationReport report,
        string moduleId,
        IReadOnlyList<GameModuleContentDeclaration> contents,
        IReadOnlyList<GameModuleAssetDeclaration> assets,
        IReadOnlyList<ScheduledSystemDeclaration> systems)
    {
        if (string.IsNullOrWhiteSpace(moduleId))
        {
            return;
        }

        foreach (var content in contents)
        {
            RequireModuleOwnedId(report, moduleId, content.ContentId, "module.content.id.owner.invalid", "Content ID");
        }

        foreach (var asset in assets)
        {
            RequireModuleOwnedId(report, moduleId, asset.AssetId, "module.asset.id.owner.invalid", "Asset ID");
        }

        foreach (var system in systems)
        {
            RequireModuleOwnedId(report, moduleId, system.SystemId, "module.schedule.system_id.owner.invalid", "Scheduled system ID");
            foreach (var resource in system.Reads ?? [])
            {
                if (!string.IsNullOrWhiteSpace(resource.ResourceId) &&
                    resource.ResourceId.StartsWith("host.", StringComparison.Ordinal))
                {
                    if (!s_allowedHostReadResources.Contains(resource.ResourceId))
                    {
                        report.AddError(
                            "module.schedule.read.host_resource.invalid",
                            $"Scheduled system reads an unexposed host resource. System: {system.SystemId} Resource: {resource.ResourceId}");
                    }

                    continue;
                }

                RequireModuleOwnedId(report, moduleId, resource.ResourceId, "module.schedule.read.resource_id.owner.invalid", "Scheduled read resource ID");
            }

            foreach (var resource in system.Writes ?? [])
            {
                if (!string.IsNullOrWhiteSpace(resource.ResourceId) &&
                    resource.ResourceId.StartsWith("host.", StringComparison.Ordinal))
                {
                    if (!s_allowedHostWriteResources.Contains(resource.ResourceId))
                    {
                        report.AddError(
                            "module.schedule.write.host_resource.invalid",
                            $"Scheduled system writes an unexposed host resource. System: {system.SystemId} Resource: {resource.ResourceId}");
                    }

                    continue;
                }

                RequireModuleOwnedId(report, moduleId, resource.ResourceId, "module.schedule.write.resource_id.owner.invalid", "Scheduled write resource ID");
            }
        }
    }

    private static bool HasScheduledWrite(
        IReadOnlyList<ScheduledSystemDeclaration> systems,
        string resourceId)
    {
        return systems.Any(system => (system.Writes ?? [])
            .Any(resource => resource.ResourceId == resourceId && resource.Mode == ScheduledAccessMode.Write));
    }

    private static bool HasScheduledRead(
        IReadOnlyList<ScheduledSystemDeclaration> systems,
        string resourceId)
    {
        return systems.Any(system => (system.Reads ?? [])
            .Any(resource => resource.ResourceId == resourceId && resource.Mode == ScheduledAccessMode.Read));
    }

    private static void RequireModuleOwnedId(
        ModuleValidationReport report,
        string moduleId,
        string value,
        string code,
        string label)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return;
        }

        if (!value.StartsWith($"{moduleId}.", StringComparison.Ordinal))
        {
            report.AddError(code, $"{label} must be module-owned. Module: {moduleId} Value: {value}");
        }
    }

    private static void RequireResourceAccess(
        ModuleValidationReport report,
        string systemId,
        IReadOnlyList<ScheduledResourceAccess> resources,
        string direction)
    {
        foreach (var resource in resources)
        {
            RequireText(
                report,
                resource.ResourceId,
                $"module.schedule.{direction}.resource_id.required",
                $"Scheduled system {direction} resource ID is required. System: {systemId}");
            if (resource.Mode == 0)
            {
                report.AddError(
                    $"module.schedule.{direction}.mode.required",
                    $"Scheduled system {direction} resource mode is required. System: {systemId}");
                continue;
            }

            if (!Enum.IsDefined(resource.Mode))
            {
                report.AddError(
                    $"module.schedule.{direction}.mode.invalid",
                    $"Scheduled system {direction} resource mode is invalid. System: {systemId}");
                continue;
            }

            var expectedMode = direction == "read"
                ? ScheduledAccessMode.Read
                : ScheduledAccessMode.Write;
            if (resource.Mode != expectedMode)
            {
                report.AddError(
                    $"module.schedule.{direction}.mode.mismatch",
                    $"Scheduled system {direction} resource mode must be {expectedMode}. System: {systemId} Resource: {resource.ResourceId}");
            }
        }
    }

    private static void RequireNoResourceOverlap(
        ModuleValidationReport report,
        ScheduledSystemDeclaration system)
    {
        var reads = (system.Reads ?? [])
            .Select(resource => resource.ResourceId)
            .Where(resourceId => !string.IsNullOrWhiteSpace(resourceId))
            .ToHashSet(StringComparer.Ordinal);

        foreach (var resourceId in (system.Writes ?? []).Select(resource => resource.ResourceId))
        {
            if (reads.Contains(resourceId))
            {
                report.AddError(
                    "module.schedule.resource.read_write_overlap",
                    $"Scheduled system cannot declare the same resource as read and write. System: {system.SystemId} Resource: {resourceId}");
            }
        }
    }
}
