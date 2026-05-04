using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;

internal static class Fixtures
{
    public static GameModuleManifest ValidManifest(
        string contentId = "octaryn.test.content",
        string contentPath = "Data/Rules/octaryn.test.rule.json",
        string contentKind = "rule",
        string assetId = "octaryn.test.asset",
        string assetKind = "texture",
        string assetPath = "Assets/Textures/octaryn.test.texture.txt",
        string minimumHostApiVersion = "0.1.0",
        string maximumHostApiVersion = "0.1.0",
        IReadOnlyList<string>? requiredCapabilities = null,
        IReadOnlyList<string>? requestedHostApis = null,
        IReadOnlyList<ScheduledSystemDeclaration>? systems = null)
    {
        var hostApis = requestedHostApis ??
        [
            HostApiIds.Commands,
            HostApiIds.Frame
        ];

        return new GameModuleManifest(
            ModuleId: "octaryn.test",
            DisplayName: "Octaryn Test Module",
            Version: "0.1.0",
            OctarynApiVersion: "0.1.0",
            RequiredCapabilities: requiredCapabilities ??
            [
                ModuleCapabilityIds.ContentBlocks,
                ModuleCapabilityIds.ContentItems,
                ModuleCapabilityIds.GameplayRules,
                ModuleCapabilityIds.WorldBlockEdits
            ],
            RequestedHostApis: hostApis,
            RequestedRuntimePackages: [],
            RequestedBuildPackages: [],
            RequestedFrameworkApiGroups:
            [
                FrameworkApiGroupIds.BclPrimitives
            ],
            ModuleDependencies: [],
            ContentDeclarations:
            [
                new GameModuleContentDeclaration(contentId, contentKind, contentPath)
            ],
            AssetDeclarations:
            [
                new GameModuleAssetDeclaration(assetId, assetKind, assetPath)
            ],
            Schedule: new GameModuleScheduleDeclaration(systems ?? [ScheduledSystem("octaryn.test.tick", includeCommandWrite: hostApis.Contains(HostApiIds.Commands, StringComparer.Ordinal))]),
            Compatibility: new GameModuleCompatibility(
                MinimumHostApiVersion: minimumHostApiVersion,
                MaximumHostApiVersion: maximumHostApiVersion,
                SaveCompatibilityId: "octaryn.test.save.v0",
                SupportsMultiplayer: false));
    }

    public static ScheduledSystemDeclaration ScheduledSystem(
        string systemId,
        IReadOnlyList<ScheduledResourceAccess>? reads = null,
        IReadOnlyList<ScheduledResourceAccess>? writes = null,
        IReadOnlyList<string>? runsAfter = null,
        bool includeCommandWrite = true)
    {
        return new ScheduledSystemDeclaration(
            SystemId: systemId,
            Phase: HostWorkPhase.Gameplay,
            FrameOrTickOwner: HostScheduleIds.FrameOrTickOwner,
            Reads: reads ??
            [
                new ScheduledResourceAccess("host.frame", ScheduledAccessMode.Read)
            ],
            Writes: writes ?? DefaultWrites(includeCommandWrite),
            RunsAfter: runsAfter ?? [],
            RunsBefore: [],
            Flags: HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
            CommitBarrier: HostScheduleIds.FrameOrTickEndBarrier);
    }

    public static void WriteValidManifestFiles(string moduleRoot)
    {
        WriteFile(moduleRoot, "Data/Rules/octaryn.test.rule.json", """
            {"id":"octaryn.test.content","kind":"rule"}
            """);
        WriteFile(moduleRoot, "Assets/Textures/octaryn.test.texture.txt", "asset");
    }

    public static void WriteFile(string moduleRoot, string relativePath, string text)
    {
        var path = Path.Combine(moduleRoot, relativePath);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, text);
    }

    private static IReadOnlyList<ScheduledResourceAccess> DefaultWrites(bool includeCommandWrite)
    {
        var writes = new List<ScheduledResourceAccess>
        {
            new("octaryn.test.state", ScheduledAccessMode.Write)
        };
        if (includeCommandWrite)
        {
            writes.Add(new ScheduledResourceAccess(HostApiIds.Commands, ScheduledAccessMode.Write));
        }

        return writes;
    }
}
