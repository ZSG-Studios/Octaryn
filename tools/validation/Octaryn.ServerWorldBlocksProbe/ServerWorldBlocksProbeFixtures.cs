using Octaryn.Basegame.Gameplay.Interaction;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static HostFrameSnapshot Frame(ulong frameIndex)
    {
        return new HostFrameSnapshot(
            new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
            new HostFrameTimingSnapshot(
                HostFrameTimingSnapshot.VersionValue,
                HostFrameTimingSnapshot.SizeValue,
                frameIndex,
                deltaSeconds: 1.0 / 60.0));
    }

    private static string ResetProbeDirectory(string name)
    {
        var root = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PROBE_DIR");
        if (string.IsNullOrWhiteSpace(root))
        {
            root = DefaultProbeRoot();
        }

        var directory = Path.Combine(root, name);
        if (Directory.Exists(directory))
        {
            Directory.Delete(directory, recursive: true);
        }

        Directory.CreateDirectory(directory);
        return directory;
    }

    private static string? UseProbePersistenceFile(string name)
    {
        var directory = ResetProbeDirectory(name);
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        SetPersistencePath(Path.Combine(directory, "world_blocks.json"));
        return previousPath;
    }

    private static void SetPersistencePath(string path)
    {
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);
    }

    private static void RestorePersistencePath(string? previousPath)
    {
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", previousPath);
    }

    private static string DefaultProbeRoot()
    {
        var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        if (string.IsNullOrWhiteSpace(presetName))
        {
            presetName = "debug-linux";
        }

        return Path.Combine("build", presetName, "server", "validation", "server-world-blocks-probe");
    }

    private static int UnpackLow(ulong value)
    {
        return unchecked((int)(uint)value);
    }

    private static int UnpackHigh(ulong value)
    {
        return unchecked((int)(uint)(value >> 32));
    }

    private static void Require(bool condition, string label)
    {
        if (!condition)
        {
            throw new InvalidOperationException($"Server world blocks probe failed: {label}.");
        }
    }

    private sealed class BlockEditRegistration : IGameModuleRegistration, IBlockAuthorityRulesProvider
    {
        public IBlockAuthorityRules BlockAuthorityRules { get; } = new BlockAuthorityRules();

        public GameModuleManifest Manifest { get; } = new(
            ModuleId: "octaryn.probe.server_world_blocks",
            DisplayName: "Octaryn Server World Blocks Probe",
            Version: "0.1.0",
            OctarynApiVersion: "0.1.0",
            RequiredCapabilities:
            [
                ModuleCapabilityIds.GameplayRules,
                ModuleCapabilityIds.WorldBlockEdits
            ],
            RequestedHostApis:
            [
                HostApiIds.Commands,
                HostApiIds.Frame
            ],
            RequestedRuntimePackages: [],
            RequestedBuildPackages: [],
            RequestedFrameworkApiGroups: [],
            ModuleDependencies: [],
            ContentDeclarations: [],
            AssetDeclarations: [],
            Schedule: new GameModuleScheduleDeclaration(
            [
                new ScheduledSystemDeclaration(
                    SystemId: "octaryn.probe.server_world_blocks.tick",
                    Phase: HostWorkPhase.Gameplay,
                    FrameOrTickOwner: HostScheduleIds.FrameOrTickOwner,
                    Reads:
                    [
                        new ScheduledResourceAccess(HostApiIds.Frame, ScheduledAccessMode.Read)
                    ],
                    Writes:
                    [
                        new ScheduledResourceAccess(HostApiIds.Commands, ScheduledAccessMode.Write)
                    ],
                    RunsAfter: [],
                    RunsBefore: [],
                    Flags: HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
                    CommitBarrier: HostScheduleIds.FrameOrTickEndBarrier)
            ]),
            Compatibility: new GameModuleCompatibility(
                MinimumHostApiVersion: "0.1.0",
                MaximumHostApiVersion: "0.1.0",
                SaveCompatibilityId: "octaryn.probe.server_world_blocks.save.v0",
                SupportsMultiplayer: false));

        public IGameModuleInstance CreateInstance(ModuleHostContext context)
        {
            return new BlockEditModule(context);
        }
    }

    private sealed class BlockEditModule(ModuleHostContext context) : IGameModuleInstance
    {
        public void Tick(in ModuleFrameContext frame)
        {
            _ = frame;
            if (!context.Commands.TryRequest(ModuleCommandRequest.SetBlock(
                new BlockEdit(new BlockPosition(8, 9, 10), new BlockId(5)),
                requestId: 12)))
            {
                throw new InvalidOperationException("server world blocks probe command was rejected.");
            }
        }

        public void Dispose()
        {
        }
    }

    private sealed class RejectingCommandSink : IHostCommandSink
    {
        public bool Enqueue(HostCommand command)
        {
            _ = command;
            return false;
        }
    }
}
