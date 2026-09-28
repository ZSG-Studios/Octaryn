using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.GameModules;

namespace Octaryn.Basegame.Module;

public sealed class ModuleRegistration : IGameModuleRegistration
{
    public GameModuleManifest Manifest { get; } = new(
        ModuleId: "octaryn.basegame",
        DisplayName: "Octaryn Basegame",
        Version: "0.1.0",
        OctarynApiVersion: "0.1.0",
        RequiredCapabilities:
        [
            ModuleCapabilityIds.GameplayRules
        ],
        RequestedHostApis:
        [
            HostApiIds.Frame,
            HostApiIds.Time,
            HostApiIds.Diagnostics,
            HostApiIds.Physics,
            HostApiIds.World,
            HostApiIds.Player,
            HostApiIds.Ecs,
            HostApiIds.Input,
            HostApiIds.Scheduling,
            HostApiIds.Ui,
            HostApiIds.Replication,
            HostApiIds.Audio
        ],
        RequestedRuntimePackages:
        [],
        RequestedBuildPackages:
        [],
        RequestedFrameworkApiGroups:
        [
            FrameworkApiGroupIds.BclPrimitives,
            FrameworkApiGroupIds.BclCollections,
            FrameworkApiGroupIds.BclMemory,
            FrameworkApiGroupIds.BclMath,
            FrameworkApiGroupIds.BclTime,
            FrameworkApiGroupIds.BclText
        ],
        ModuleDependencies: [],
        ContentDeclarations:
        [],
        AssetDeclarations:
        [
            new GameModuleAssetDeclaration(
                "octaryn.basegame.audio.actions",
                "audio",
                "Assets/Audio/action-sounds.json")
        ],
        Schedule: new GameModuleScheduleDeclaration(
        [
            ScheduleDeclarations.FrameTick
        ]),
        Compatibility: new GameModuleCompatibility(
            MinimumHostApiVersion: "0.1.0",
            MaximumHostApiVersion: "0.1.0",
            SaveCompatibilityId: "octaryn.basegame.save.v0",
            SupportsMultiplayer: false));

    public IGameModuleInstance CreateInstance(ModuleHostContext context)
    {
        return GameContext.Create(context);
    }
}
