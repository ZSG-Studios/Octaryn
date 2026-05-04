using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;

internal static class SelfTests
{
    public static List<string> Run()
    {
        var errors = new List<string>();
        ExpectInvalid(errors, "content path owner", Fixtures.ValidManifest(contentPath: "Assets/x.json"), "module.content.path.owner.invalid");
        ExpectInvalid(errors, "asset path owner", Fixtures.ValidManifest(assetPath: "Data/x.txt"), "module.asset.path.owner.invalid");
        ExpectInvalid(errors, "path traversal", Fixtures.ValidManifest(contentPath: "Data/../x.json"), "module.content.path.invalid");
        ExpectInvalid(errors, "absolute path", Fixtures.ValidManifest(contentPath: "/Data/x.json"), "module.content.path.invalid");
        ExpectInvalid(errors, "colon path", Fixtures.ValidManifest(contentPath: "Data/C:/x.json"), "module.content.path.invalid");
        ExpectInvalid(errors, "duplicate declaration ID", Fixtures.ValidManifest(assetId: "octaryn.test.content"), "module.declaration.id.duplicate");
        ExpectInvalid(errors, "non-owned content ID", Fixtures.ValidManifest(contentId: "other.content"), "module.content.id.owner.invalid");
        ExpectInvalid(
            errors,
            "non-owned write resource",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", writes: [new ScheduledResourceAccess("other.state", ScheduledAccessMode.Write)])
            ]),
            "module.schedule.write.resource_id.owner.invalid");
        ExpectInvalid(
            errors,
            "non-owned read resource",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", reads: [new ScheduledResourceAccess("other.state", ScheduledAccessMode.Read)])
            ]),
            "module.schedule.read.resource_id.owner.invalid");
        ExpectInvalid(
            errors,
            "unexposed host write resource",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", writes: [new ScheduledResourceAccess("host.private", ScheduledAccessMode.Write)])
            ]),
            "module.schedule.write.host_resource.invalid");
        ExpectInvalid(
            errors,
            "commands requested without scheduled write",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", includeCommandWrite: false)
            ]),
            "module.schedule.commands.write.required");
        ExpectInvalid(
            errors,
            "commands requested without block edit capability",
            Fixtures.ValidManifest(requiredCapabilities:
            [
                ModuleCapabilityIds.ContentBlocks,
                ModuleCapabilityIds.ContentItems,
                ModuleCapabilityIds.GameplayRules
            ]),
            "module.capability.world_block_edits.required");
        ExpectInvalid(
            errors,
            "scheduled command write without block edit capability",
            Fixtures.ValidManifest(
                requiredCapabilities:
                [
                    ModuleCapabilityIds.ContentBlocks,
                    ModuleCapabilityIds.ContentItems,
                    ModuleCapabilityIds.GameplayRules
                ],
                requestedHostApis: [HostApiIds.Frame],
                systems:
                [
                    Fixtures.ScheduledSystem("octaryn.test.tick")
                ]),
            "module.capability.world_block_edits.required");
        ExpectInvalid(
            errors,
            "unexposed host read resource",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", reads: [new ScheduledResourceAccess("host.private", ScheduledAccessMode.Read)])
            ]),
            "module.schedule.read.host_resource.invalid");
        ExpectInvalid(
            errors,
            "read resource write mode",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem(
                    "octaryn.test.tick",
                    reads: [new ScheduledResourceAccess("host.frame", ScheduledAccessMode.Write)])
            ]),
            "module.schedule.read.mode.mismatch");
        ExpectInvalid(
            errors,
            "write resource read mode",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem(
                    "octaryn.test.tick",
                    writes: [new ScheduledResourceAccess("octaryn.test.state", ScheduledAccessMode.Read)])
            ]),
            "module.schedule.write.mode.mismatch");
        ExpectInvalid(errors, "invalid content kind", Fixtures.ValidManifest(contentKind: "mesh"), "module.content.kind.invalid");
        ExpectInvalid(errors, "invalid asset kind", Fixtures.ValidManifest(assetKind: "save"), "module.asset.kind.invalid");
        ExpectInvalid(
            errors,
            "invalid version range",
            Fixtures.ValidManifest(minimumHostApiVersion: "0.2.0", maximumHostApiVersion: "0.1.0"),
            "module.compatibility.host_api_range.invalid");
        ExpectInvalid(errors, "missing frame API for scheduled frame read", Fixtures.ValidManifest(requestedHostApis: [HostApiIds.Commands]), "module.schedule.frame.read.required");
        ExpectInvalid(
            errors,
            "self dependency",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", runsAfter: ["octaryn.test.tick"])
            ]),
            "module.schedule.runs_after.self");
        ExpectInvalid(
            errors,
            "unknown dependency",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", runsAfter: ["octaryn.test.missing"])
            ]),
            "module.schedule.runs_after.unknown");
        ExpectInvalid(
            errors,
            "schedule graph cycle",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.first", runsAfter: ["octaryn.test.second"]),
                Fixtures.ScheduledSystem("octaryn.test.second", runsAfter: ["octaryn.test.first"])
            ]),
            "module.schedule.graph.cycle");
        ExpectInvalid(
            errors,
            "read write resource overlap",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.tick", reads: [new ScheduledResourceAccess("octaryn.test.state", ScheduledAccessMode.Read)])
            ]),
            "module.schedule.resource.read_write_overlap");
        ExpectInvalid(
            errors,
            "unordered cross-system write conflict",
            Fixtures.ValidManifest(systems:
            [
                Fixtures.ScheduledSystem("octaryn.test.first"),
                Fixtures.ScheduledSystem("octaryn.test.second")
            ]),
            "module.schedule.resource.conflict_unordered");
        ExpectFileGraphInvalid(errors, "missing declared content file", Fixtures.ValidManifest(), "declared file missing");
        ExpectFileGraphInvalid(
            errors,
            "empty declared content file",
            Fixtures.ValidManifest(),
            "declared file is empty",
            root => Fixtures.WriteFile(root, "Data/Rules/octaryn.test.rule.json", string.Empty));
        ExpectFileGraphInvalid(
            errors,
            "missing declared content identity",
            Fixtures.ValidManifest(),
            "declared content id mismatch",
            root => Fixtures.WriteFile(root, "Data/Rules/octaryn.test.rule.json", "{}"));
        ExpectFileGraphInvalid(
            errors,
            "mismatched declared content id",
            Fixtures.ValidManifest(),
            "declared content id mismatch",
            root => Fixtures.WriteFile(root, "Data/Rules/octaryn.test.rule.json", """
                {"id":"octaryn.test.other","kind":"rule"}
                """));
        ExpectFileGraphInvalid(
            errors,
            "mismatched declared content kind",
            Fixtures.ValidManifest(),
            "declared content kind mismatch",
            root => Fixtures.WriteFile(root, "Data/Rules/octaryn.test.rule.json", """
                {"id":"octaryn.test.content","kind":"item"}
                """));
        ExpectFileGraphInvalid(
            errors,
            "undeclared data file",
            Fixtures.ValidManifest(),
            "undeclared content file",
            Fixtures.WriteValidManifestFiles,
            root => Fixtures.WriteFile(root, "Data/Rules/octaryn.test.extra.json", "{}"));
        ExpectFileGraphInvalid(
            errors,
            "undeclared asset file",
            Fixtures.ValidManifest(),
            "undeclared asset file",
            Fixtures.WriteValidManifestFiles,
            root => Fixtures.WriteFile(root, "Assets/Textures/octaryn.test.extra.txt", "asset"));
        ExpectFileGraphInvalid(
            errors,
            "undeclared shader file",
            Fixtures.ValidManifest(),
            "undeclared shader file",
            Fixtures.WriteValidManifestFiles,
            root => Fixtures.WriteFile(root, "Shaders/octaryn.test.shader.glsl", "void main() {}"));

        return errors;
    }

    private static void ExpectInvalid(
        List<string> errors,
        string name,
        GameModuleManifest manifest,
        string expectedCode)
    {
        var report = GameModuleValidator.Validate(manifest);
        if (!report.Issues.Any(issue => issue.Code == expectedCode))
        {
            errors.Add($"{name}: expected {expectedCode}, got {string.Join(", ", report.Issues.Select(issue => issue.Code))}");
        }
    }

    private static void ExpectFileGraphInvalid(
        List<string> errors,
        string name,
        GameModuleManifest manifest,
        string expectedText,
        params Action<string>[] setup)
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), $"octaryn-module-manifest-probe-{Guid.NewGuid():N}");
        try
        {
            Directory.CreateDirectory(Path.Combine(tempRoot, "Data"));
            Directory.CreateDirectory(Path.Combine(tempRoot, "Assets"));
            Directory.CreateDirectory(Path.Combine(tempRoot, "Shaders"));
            foreach (var action in setup)
            {
                action(tempRoot);
            }

            var validationErrors = ManifestValidator.Validate(tempRoot, manifest);
            if (!validationErrors.Any(error => error.Contains(expectedText, StringComparison.Ordinal)))
            {
                errors.Add($"{name}: expected {expectedText}, got {string.Join(", ", validationErrors)}");
            }
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }
}
