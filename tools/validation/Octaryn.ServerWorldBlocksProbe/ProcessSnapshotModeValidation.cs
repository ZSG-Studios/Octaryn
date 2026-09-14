using Octaryn.Server;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private const int ProcessBatchEdits = 8193;
    private static readonly BlockPosition ProcessFarBlock = new(320, 120, -320);

    private static unsafe void ValidateProcessSnapshotMode()
    {
        var invalidRejected = false;
        try { using var invalid = new ModuleActivator(new BlockEditRegistration(), (BlockPublicationMode)(-1)); }
        catch (ArgumentOutOfRangeException) { invalidRejected = true; }
        Require(invalidRejected, "unknown publication mode must fail explicitly");
        var root = ResetProbeDirectory("process-mode");
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        SetPersistencePath(Path.Combine(root, "world_blocks.json"));
        try
        {
            using (var activator = new ModuleActivator(new ProcessBatchRegistration(), BlockPublicationMode.ProcessSnapshots))
            {
                Require(activator.PublicationMode == BlockPublicationMode.ProcessSnapshots, "explicit process publication mode");
                Require(activator.Activate(new RejectingCommandSink()) == 0, "process mode activation");
                activator.Tick(Frame(1)); // Entire >8192 edit batch, followed by one dirty save.
                Require(activator.WorldBlockCount == ProcessBatchEdits + 1 &&
                    activator.BlockRevision == ProcessBatchEdits + 1 && activator.PendingBlockChangeCount == 0,
                    "large module batch applies and persists without accumulating unused deltas");
                var command = new HostCommand {
                    Version = HostCommand.VersionValue, Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock, A = 1, B = 130, C = 1, D = 5
                };
                Require(activator.SubmitClientCommands(&command, 1) == 0 &&
                    activator.BlockRevision == ProcessBatchEdits + 1, "process mode queue acceptance is not application");
                activator.Tick(Frame(2));
                Require(activator.GetBlock(new(1, 130, 1)).Value == 5 &&
                    activator.BlockRevision == ProcessBatchEdits + 2 && activator.PendingBlockChangeCount == 0,
                    "queued process client edit applies and advances revision without delta queue");
                var header = new ServerSnapshotHeader(0, 17, 19, 0, 0);
                Require(activator.DrainServerSnapshots(&header) == ModuleActivator.DeltaSnapshotsUnsupported &&
                    header.ChangeCount == 17 && header.TickId == 19,
                    "process authority explicitly rejects delta drain without claiming empty history");

                var window = new NativeChunkViewIntent(1, 60, 0, 0, 1, 0, 0, 0, 0);
                var path = Path.Combine(root, "stream.json");
                var blockedBinary = path + ".bin";
                Directory.CreateDirectory(blockedBinary);
                var marker = Path.Combine(blockedBinary, "preserve.txt");
                File.WriteAllText(marker, "bounded fixture blocks native publication");
                var failed = false;
                try { ChunkStreamProcessBridge.PublishSnapshot(activator, path, window, true, false, true); }
                catch (InvalidOperationException) { failed = true; }
                Require(failed && activator.ChunkPublication.NeedsFullSnapshot(path, activator.BlockRevision) &&
                    activator.PendingBlockChangeCount == 0 && activator.WorldBlockCount == ProcessBatchEdits + 2,
                    "failed process writer retains authority and revision without buffering deltas");
                File.Delete(marker);
                Directory.Delete(blockedBinary);
                Require(ChunkStreamProcessBridge.PublishSnapshot(activator, path, window, true, false, true) == 0,
                    "process mode retries actual full native publication");
                var near = ReadPublicationBlocks(path);
                Require(near.Length == ProcessBatchEdits + 1 && near.Contains((1, 130, 1, 5)) &&
                    !near.Any(edit => edit.X == ProcessFarBlock.X && edit.Z == ProcessFarBlock.Z),
                    "near snapshot contains near edits without pretending to publish far edit");
                Require(activator.GetBlock(ProcessFarBlock).Value == 5 && activator.PendingBlockChangeCount == 0,
                    "out-of-window edit remains authoritative after near publication");
                window = new NativeChunkViewIntent(1, 61, 10, -10, 1, 1, 0, 0, 1);
                Require(ChunkStreamProcessBridge.PublishSnapshot(activator, path, window, true, false, true) == 0,
                    "moving process window publishes retained distant edit");
                var far = ReadPublicationBlocks(path);
                Require(far.Length == 1 && far[0] == (320, 120, -320, 5),
                    "later full snapshot contains exact far authoritative edit");
            }
            using (var restored = new ModuleActivator(new ProcessBatchRegistration(), BlockPublicationMode.ProcessSnapshots))
            {
                Require(restored.Activate(new RejectingCommandSink()) == 0, "process mode saved authority reopens");
                Require(restored.WorldBlockCount == ProcessBatchEdits + 2 && restored.GetBlock(ProcessFarBlock).Value == 5 &&
                    restored.GetBlock(new(1, 130, 1)).Value == 5 && restored.PendingBlockChangeCount == 0,
                    "large process batch and client/far edits survive persistence reopen");
                Require(restored.ChunkPublication.NeedsFullSnapshot("restored", restored.BlockRevision),
                    "restored process authority starts without a publication acknowledgement");
            }
        }
        finally { RestorePersistencePath(previousPath); }
        ValidateDefaultPublicationMode();
        Console.WriteLine($"process_snapshot_mode=passed module_batch={ProcessBatchEdits + 1} client_edit=passed delta_pending=0 native_retry=passed distant_edit=passed persistence=passed default_deltas=passed");
    }

    private static unsafe void ValidateDefaultPublicationMode()
    {
        var previousPath = UseProbePersistenceFile("default-publication-mode");
        try
        {
            using var activator = new ModuleActivator(new BlockEditRegistration());
            Require(activator.PublicationMode == BlockPublicationMode.ReplicationDeltas,
                "existing authority constructors retain delta replication default");
            Require(activator.Activate(new RejectingCommandSink()) == 0, "default mode activation");
            activator.Tick(Frame(1));
            Require(activator.PendingBlockChangeCount == 1, "default mode preserves real module delta");
            var change = default(ReplicationChange);
            var header = new ServerSnapshotHeader(0, 1, 0, 0, (ulong)&change);
            Require(activator.DrainServerSnapshots(&header) == 0 && header.ChangeCount == 1 &&
                UnpackLow(change.Payload0) == 8 && UnpackHigh(change.Payload0) == 9 &&
                UnpackLow(change.Payload1) == 10 && (ushort)(change.Payload1 >> 32) == 5 &&
                activator.PendingBlockChangeCount == 0, "default mode drains exact authoritative delta normally");
        }
        finally { RestorePersistencePath(previousPath); }
    }

    private sealed class ProcessBatchRegistration : IGameModuleRegistration, IBlockAuthorityRulesProvider
    {
        private readonly BlockEditRegistration _definition = new();
        public IBlockAuthorityRules BlockAuthorityRules => _definition.BlockAuthorityRules;
        public GameModuleManifest Manifest => _definition.Manifest;
        public IGameModuleInstance CreateInstance(ModuleHostContext context) => new ProcessBatchModule(context);
    }

    private sealed class ProcessBatchModule(ModuleHostContext context) : IGameModuleInstance
    {
        private bool _applied;
        public void Tick(in ModuleFrameContext frame)
        {
            if (_applied) return;
            _applied = true;
            for (var index = 0; index < ProcessBatchEdits; ++index)
            {
                var position = new BlockPosition(index % 32, 100 + index / 1024, index / 32 % 32);
                Require(context.Commands.TryRequest(ModuleCommandRequest.SetBlock(
                    new BlockEdit(position, new BlockId(5)), (ulong)index + 1000)), "process module batch edit accepted");
            }
            Require(context.Commands.TryRequest(ModuleCommandRequest.SetBlock(
                new BlockEdit(ProcessFarBlock, new BlockId(5)), 20000)), "process module distant edit accepted");
        }
        public void Dispose() { }
    }
}
