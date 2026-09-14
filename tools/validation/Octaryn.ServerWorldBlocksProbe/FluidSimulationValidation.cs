using Octaryn.Basegame.Content.Fluids;
using Octaryn.Server;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidateFluidSimulation()
    {
        ValidateFluidFall(false);
        ValidateFluidFall(true);
        ValidateBasegameFluidProfile();
        Console.WriteLine("fluid_simulation=passed water_delay=passed lava_delay=passed native_apply=passed publication=passed persistence=passed");
    }

    private static void ValidateFluidFall(bool lava)
    {
        var previous = UseProbePersistenceFile(lava ? "lava-simulation" : "water-simulation");
        var rules = BasegameFluidRules.Create();
        var levels = lava ? rules.LavaLevels : rules.WaterLevels;
        var source = new BlockPosition(8, 0, 8);
        var below = new BlockPosition(8, -1, 8);
        try
        {
            using (var authority = new ModuleActivator(new FlowRegistration(), BlockPublicationMode.ProcessSnapshots))
            {
                Require(authority.Activate(new RejectingCommandSink()) == 0, "fluid authority activation");
                var unconfiguredSource = new BlockPosition(16, 0, 16);
                Require(authority.SubmitClientCommands(new[] { FlowCommand(unconfiguredSource, levels[0]) }) == 0, "fluid source queued");
                authority.Tick(FlowFrame(1, 0.25));
                Require(authority.GetBlock(unconfiguredSource) == levels[0] && authority.GetBlock(new BlockPosition(16, -1, 16)) == BlockId.Air,
                    "no fluid simulation without configured region");
                authority.SetFluidRegion(0, 0, 0);
                Require(authority.SubmitClientCommands(new[] { FlowCommand(source, levels[0]) }) == 0, "resident source queued");
                authority.Tick(FlowFrame(3, 0));
                var revision = authority.BlockRevision;
                var path = Path.Combine(Path.GetDirectoryName(Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH"))!, "stream.json");
                var window = new NativeChunkViewIntent(1, 70, 0, 0, 0, 0, 0, 0, 0);
                Require(ChunkStreamProcessBridge.PublishSnapshot(authority, path, window, false, true, true) == 0,
                    "fluid source baseline published");
                authority.Tick(FlowFrame(4, 0.249));
                Require(authority.GetBlock(below) == BlockId.Air, "fluid does not fall before water delay");
                if (lava)
                {
                    authority.Tick(FlowFrame(5, 0.249));
                    Require(authority.GetBlock(below) == BlockId.Air, "lava retains its longer delay");
                }
                authority.Tick(FlowFrame(6, 0.002));
                // Budgets may spread work across calls, but a held simulation clock
                // must eventually finish already-due positions without aging them.
                for (var attempt = 0; attempt < 32 && authority.GetBlock(below) == BlockId.Air; attempt++)
                    authority.Tick(FlowFrame((ulong)attempt + 7, 0));
                Require(authority.GetBlock(below) == levels[1], "native fluid apply creates correct flowing level");
                Require(authority.BlockRevision > revision && authority.PendingBlockChangeCount == 0,
                    "autonomous fluid changes advance revision without unused deltas");
                Require(ChunkStreamProcessBridge.PublishSnapshot(authority, path, window, false, false, true) == 0,
                    "autonomous flow publishes without new client command");
                Require(ReadPublicationBlocks(path).Contains((8, -1, 8, levels[1].Value)),
                    "actual stream contains authoritative falling fluid");
            }
            using var loaded = new ModuleActivator(new FlowRegistration(), BlockPublicationMode.ProcessSnapshots);
            Require(loaded.GetBlock(source) == levels[0] && loaded.GetBlock(below) == levels[1],
                "source and simulated flow survive save reload");
        }
        finally { RestorePersistencePath(previous); }
    }

    private static HostCommand FlowCommand(BlockPosition position, BlockId block) => new()
    {
        Version = HostCommand.VersionValue, Size = HostCommand.SizeValue,
        Kind = HostCommandKind.SetBlock, A = position.X, B = position.Y, C = position.Z, D = block.Value
    };

    private static HostFrameSnapshot FlowFrame(ulong frame, double delta) => new(
        new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
        new HostFrameTimingSnapshot(HostFrameTimingSnapshot.VersionValue, HostFrameTimingSnapshot.SizeValue, frame, delta));

    private sealed class FlowRegistration : IGameModuleRegistration, IBlockAuthorityRulesProvider, IFluidRulesProvider
    {
        private readonly BlockEditRegistration _definition = new();
        public IBlockAuthorityRules BlockAuthorityRules => _definition.BlockAuthorityRules;
        public FluidRules FluidRules { get; } = BasegameFluidRules.Create();
        public GameModuleManifest Manifest => _definition.Manifest;
        public IGameModuleInstance CreateInstance(ModuleHostContext context) => new FlowModule();
    }

    private sealed class FlowModule : IGameModuleInstance
    {
        public void Tick(in ModuleFrameContext frame) { }
        public void Dispose() { }
    }
}
