using Octaryn.Basegame.Gameplay.Interaction;
using Octaryn.Basegame.Content.Worldgen;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Module;

public sealed class ModuleRegistration : IGameModuleRegistration, IBlockAuthorityRulesProvider, IWorldGenerationRulesProvider
{
    public IBlockAuthorityRules BlockAuthorityRules { get; } = new BlockAuthorityRules();

    public IWorldGenerationRules WorldGenerationRules { get; } = new WorldGenerationRules();

    public GameModuleManifest Manifest { get; } = new(
        ModuleId: "octaryn.basegame",
        DisplayName: "Octaryn Basegame",
        Version: "0.1.0",
        OctarynApiVersion: "0.1.0",
        RequiredCapabilities:
        [
            ModuleCapabilityIds.ContentBlocks,
            ModuleCapabilityIds.ContentItems,
            ModuleCapabilityIds.GameplayInteractions,
            ModuleCapabilityIds.GameplayRules,
            ModuleCapabilityIds.WorldBlockEdits,
            ModuleCapabilityIds.WorldgenBiomes,
            ModuleCapabilityIds.WorldgenFeatures,
            ModuleCapabilityIds.WorldgenNoise
        ],
        RequestedHostApis:
        [
            HostApiIds.Commands,
            HostApiIds.Frame
        ],
        RequestedRuntimePackages:
        [
            AllowedPackageIds.Arch,
            AllowedPackageIds.ArchSystem,
            AllowedPackageIds.ArchEventBus,
            AllowedPackageIds.ArchRelationships
        ],
        RequestedBuildPackages:
        [
            AllowedPackageIds.ArchSystemSourceGenerator
        ],
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
        [
            new GameModuleContentDeclaration(
                "octaryn.basegame.blocks",
                "block",
                "Data/Blocks/octaryn.basegame.blocks.json"),
            new GameModuleContentDeclaration(
                "octaryn.basegame.item.hand",
                "item",
                "Data/Items/octaryn.basegame.item.hand.json"),
            new GameModuleContentDeclaration(
                "octaryn.basegame.rule.default_interaction",
                "rule",
                "Data/Rules/octaryn.basegame.rule.default_interaction.json"),
            new GameModuleContentDeclaration(
                "octaryn.basegame.biomes",
                "biome",
                "Data/Biomes/octaryn.basegame.biomes.json"),
            new GameModuleContentDeclaration(
                "octaryn.basegame.features",
                "feature",
                "Data/Features/octaryn.basegame.features.json"),
            new GameModuleContentDeclaration(
                "octaryn.basegame.rule.terrain_generation",
                "rule",
                "Data/Rules/octaryn.basegame.rule.terrain_generation.json")
        ],
        AssetDeclarations:
        [
            new GameModuleAssetDeclaration(
                "octaryn.basegame.texture.atlas.color",
                "atlas",
                "Assets/Atlases/basegame-color.png"),
            new GameModuleAssetDeclaration(
                "octaryn.basegame.texture.atlas.normal",
                "atlas",
                "Assets/Atlases/basegame-normal.png"),
            new GameModuleAssetDeclaration(
                "octaryn.basegame.texture.atlas.specular",
                "atlas",
                "Assets/Atlases/basegame-specular.png"),
            new GameModuleAssetDeclaration(
                "octaryn.basegame.texture.atlas.animation",
                "atlas",
                "Assets/Atlases/basegame-animation.png"),
            new GameModuleAssetDeclaration(
                "octaryn.basegame.texture.atlas.animation_manifest",
                "atlas",
                "Assets/Atlases/basegame-animation.txt"),
            new GameModuleAssetDeclaration(
                "octaryn.basegame.texture.atlas.source_manifest",
                "atlas",
                "Assets/Atlases/basegame-color.txt")
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
