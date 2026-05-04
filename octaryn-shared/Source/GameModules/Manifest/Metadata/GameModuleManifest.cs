namespace Octaryn.Shared.GameModules;

public sealed record GameModuleManifest(
    string ModuleId,
    string DisplayName,
    string Version,
    string OctarynApiVersion,
    IReadOnlyList<string> RequiredCapabilities,
    IReadOnlyList<string> RequestedHostApis,
    IReadOnlyList<string> RequestedRuntimePackages,
    IReadOnlyList<string> RequestedBuildPackages,
    IReadOnlyList<string> RequestedFrameworkApiGroups,
    IReadOnlyList<GameModuleDependency> ModuleDependencies,
    IReadOnlyList<GameModuleContentDeclaration> ContentDeclarations,
    IReadOnlyList<GameModuleAssetDeclaration> AssetDeclarations,
    GameModuleScheduleDeclaration Schedule,
    GameModuleCompatibility Compatibility);
