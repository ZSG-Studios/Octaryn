using System.Text.Json;
using Octaryn.Server.Modules;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    public static unsafe int RunBlockReceiptQualification()
    {
        var previous = UseProbePersistenceFile("block-receipts-production");
        var path = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH")!;
        var runtime = Path.Combine(Path.GetDirectoryName(path)!, "runtime");
        var previousPlayers = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", Path.Combine(Path.GetDirectoryName(path)!, "players"));
        var position = new BlockPosition(-2, 76, 1);
        var support = new BlockPosition(-3, 76, 1);
        var far = new BlockPosition(100, 76, 1);
        var options = new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase };
        BlockReceiptBatch Read(ModuleActivator module) =>
            JsonSerializer.Deserialize<BlockReceiptBatch>(File.ReadAllText(module.BlockResultsPath!), options)!;
        HostCommand Command(ulong id, int block) => new()
        {
            Version = HostCommand.VersionValue, Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock, RequestId = id,
            A = position.X, B = position.Y, C = position.Z, D = block
        };
        HostCommand Interaction(ModuleActivator module, ulong id, int block, BlockPosition edit, BlockPosition hit)
        {
            var command = Command(id, block);
            var player = module.SnapshotPlayer();
            command.Flags = HostCommand.ClientInteractionFlag;
            command.A = edit.X;command.B = edit.Y;command.C = edit.Z;
            command.X = player.X;command.Y = player.Y;command.Z = player.Z;
            command.X2 = hit.X;command.Y2 = hit.Y;command.Z2 = hit.Z;
            return command;
        }
        try
        {
            NativeWorldPersistenceLibrary.EnsureWorldGeneration();
            using (var baseline = new BlockStore())
            using (var persistence = WorldBlockPersistence.FromEnvironment())
            {
                baseline.SetBlock(new(support, new(5)));
                baseline.SetBlock(new(far, new(5)));
                persistence.MarkDirty();persistence.SaveIfDirty(baseline);
            }
            using (var module = new ModuleActivator(new BlockEditRegistration()))
            {
                var session = module.BeginBlockReceiptSession(runtime);
                Require(Read(module).Session == session, "production session nonce published");
                var invalid = Interaction(module, 1, 0, far, far);
                Require(module.AdmitBlockInteraction(in invalid, 10, 9, 100) == BlockInteractionAdmission.Deferred,
                    "production admission waits consumed movement");
                Require(module.PendingClientBlockCommandCount == 0, "waiting movement not queued");
                Require(module.AdmitBlockInteraction(in invalid, 10, 10, 101) == BlockInteractionAdmission.Ready,
                    "production admission reserves after movement");
                Require(module.SubmitClientCommands(&invalid, 1) == -2, "real native validator rejected out-of-reach break");
                Require(Read(module).Receipts.Count == 0, "submit rejection waits production save barrier");
                var frame = Frame(1);
                module.TickHostOnly(in frame);
                var rejected = Read(module).Receipts.Single();
                Require(!rejected.Accepted && rejected.CommandID == 1 && rejected.Revision == 0,
                    "production rejection delivered without revision advance");
                Require(rejected.Blocks.Single().Block == 5, "reach rejection reports unchanged authoritative target");
                var accepted = Interaction(module, 2, 5, position, support);
                Require(module.AdmitBlockInteraction(in accepted, 10, 10, 102) == BlockInteractionAdmission.Ready,
                    "production accepted command reserved");
                Require(module.SubmitClientCommands(&accepted, 1) == 0, "real command queued");
                Require(Read(module).Receipts.Count == 1, "queue consumption not acceptance");
                frame = Frame(2);module.TickHostOnly(in frame);
                var batch = Read(module);
                Require(batch.Receipts.Count == 2 && batch.Receipts[1].Accepted && batch.Receipts[1].Revision == module.BlockRevision,
                    "real drain observer and persistence publish accepted revision");
                Require(module.GetBlock(position).Value == 5 && File.Exists(path), "real authoritative edit saved");
                using (var persisted = WorldBlockPersistence.FromEnvironment())
                using (var store = new BlockStore())
                {
                    persisted.Load(store);
                    Require(store.GetBlock(position).Value == 5, "accepted receipt already backed by disk before module disposal");
                }
                Require(module.AdmitBlockInteraction(in accepted, 10, 10, 103) == BlockInteractionAdmission.AlreadyHandled,
                    "duplicate production intent cannot reexecute");

                var failedSave = Interaction(module, 3, 0, position, position);
                Require(module.AdmitBlockInteraction(in failedSave, 10, 10, 104) == BlockInteractionAdmission.Ready, "reserve save failure command");
                module.SubmitClientCommands(&failedSave, 1);
                bool saveFailed = false;
                using (var lockedSave = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.None))
                {
                    frame = Frame(3);
                    try { module.TickHostOnly(in frame); }
                    catch (IOException) { saveFailed = true; }
                    Require(saveFailed && Read(module).Receipts.Count == 2,
                        "actual locked persistence cannot publish staged acceptance");
                }
                frame = Frame(4);module.TickHostOnly(in frame);
                Require(Read(module).Receipts.Last() is { CommandID: 3, Accepted: true }, "actual persistence retry publishes retained acceptance");
                var restore = Interaction(module, 4, 5, position, support);
                Require(module.AdmitBlockInteraction(in restore, 10, 10, 105) == BlockInteractionAdmission.Ready, "reserve restore");
                module.SubmitClientCommands(&restore, 1);
                frame = Frame(5);module.TickHostOnly(in frame);

                var adjacent = new BlockPosition(support.X, support.Y, support.Z + 1);
                var rapidBreak = Interaction(module, 5, 0, position, position);
                var rapidPlace = Interaction(module, 6, 5, adjacent, support);
                Require(module.AdmitBlockInteraction(in rapidBreak, 10, 10, 106) == BlockInteractionAdmission.Ready &&
                    module.SubmitClientCommands(&rapidBreak, 1) == 0, "rapid same-column break queued");
                Require(module.AdmitBlockInteraction(in rapidPlace, 10, 10, 107) == BlockInteractionAdmission.Ready &&
                    module.SubmitClientCommands(&rapidPlace, 1) == 0, "rapid same-column place queued");
                frame = Frame(6);module.TickHostOnly(in frame);
                var rapidResults = Read(module).Receipts.Where(r => r.CommandID is 5 or 6).ToArray();
                Require(rapidResults.Length == 2 && rapidResults.All(r => r.Accepted) &&
                    module.GetBlock(position).Value == 0 && module.GetBlock(adjacent).Value == 5,
                    "rapid same-column edits produce distinct accepted receipts");
                restore = Interaction(module, 7, 5, position, support);
                Require(module.AdmitBlockInteraction(in restore, 10, 10, 108) == BlockInteractionAdmission.Ready, "reserve second restore");
                module.SubmitClientCommands(&restore, 1);
                frame = Frame(7);module.TickHostOnly(in frame);

                var timeout = Command(8, 0);
                Require(module.AdmitBlockInteraction(in timeout, 20, 10, 200) == BlockInteractionAdmission.Deferred, "timeout starts deferred");
                Require(module.AdmitBlockInteraction(in timeout, 20, 10, 2200) == BlockInteractionAdmission.Rejected, "movement timeout is explicit rejection");
                frame = Frame(8);module.TickHostOnly(in frame);
                Require(Read(module).Receipts.Last() is { Accepted: false, CommandID: 8 } && module.GetBlock(position).Value == 5,
                    "timeout receipt does not mutate authority");

                var oldQueued = Command(9, 0);
                Require(module.AdmitBlockInteraction(in oldQueued, 10, 10, 2201) == BlockInteractionAdmission.Ready, "reserve old-connection pending command");
                module.SubmitClientCommands(&oldQueued, 1);
                var replacement = module.BeginBlockReceiptSession(runtime);
                Require(replacement != session && module.PendingClientBlockCommandCount == 0 && module.GetBlock(position).Value == 5,
                    "connection reset drops only unexecuted commands");
                Require(Read(module).Receipts.Count == 0 && Read(module).Session == replacement, "replacement mailbox has fresh nonce");

                for (ulong id = 1; id <= 256; ++id)
                {
                    invalid = Command(id, ushort.MaxValue);
                    Require(module.AdmitBlockInteraction(in invalid, 0, 0, 3000) == BlockInteractionAdmission.Ready, "production bounded reservation");
                    Require(module.SubmitClientCommands(&invalid, 1) == -2, "production bounded rejection");
                }
                invalid = Command(257, ushort.MaxValue);
                Require(module.AdmitBlockInteraction(in invalid, 0, 0, 3000) == BlockInteractionAdmission.Deferred, "production ledger full backpressure");
                frame = Frame(9);module.TickHostOnly(in frame);
                Require(Read(module).Receipts.Count == 256, "production full ordered batch retained");
                File.WriteAllText(Path.Combine(runtime, "block_results_ack.json"),
                    JsonSerializer.Serialize(new BlockReceiptAck(1, replacement, 256), options));
                Require(module.AdmitBlockInteraction(in invalid, 0, 0, 3001) == BlockInteractionAdmission.Ready, "production ack releases capacity");
                module.RejectAdmittedBlockInteraction(in invalid);
                module.EndBlockReceiptSession();
                var retired = JsonSerializer.Deserialize<BlockReceiptBatch>(
                    File.ReadAllText(Path.Combine(runtime, "block_results.json")), options)!;
                Require(retired.Receipts.Count == 1 && retired.Receipts[0].CommandID == 257,
                    "acknowledged batch retired from production mailbox");

                module.BeginBlockReceiptSession(runtime);
                var filler = Enumerable.Range(0, ClientBlockCommandQueue.MaxPendingCommands)
                    .Select(index => Command(10000 + (ulong)index, 5)).ToArray();
                fixed (HostCommand* commands = filler)
                    Require(module.SubmitClientCommands(commands, (uint)filler.Length) == 0, "fill real native command queue");
                var retry = Interaction(module, 1, 0, position, position);
                Require(module.AdmitBlockInteraction(in retry, 10, 10, 4000) == BlockInteractionAdmission.Ready &&
                    module.SubmitClientCommands(&retry, 1) == -1, "queue capacity retains admitted command for retry");
                frame = Frame(10);module.TickHostOnly(in frame);
                Require(Read(module).Receipts.Count == 0, "transient native capacity must not emit rejection");
                Require(module.SubmitClientCommands(&retry, 1) == 0, "retry unchanged admitted command after capacity frees");
                frame = Frame(11);module.TickHostOnly(in frame);
                Require(Read(module).Receipts.Single() is { CommandID: 1, Accepted: true }, "capacity retry preserves correlated acceptance");
                restore = Interaction(module, 2, 5, position, support);
                Require(module.AdmitBlockInteraction(in restore, 10, 10, 4001) == BlockInteractionAdmission.Ready, "reserve final restore");
                module.SubmitClientCommands(&restore, 1);
                frame = Frame(12);module.TickHostOnly(in frame);
                module.EndBlockReceiptSession();
            }
            using var reopened = new ModuleActivator(new BlockEditRegistration());
            Require(reopened.GetBlock(position).Value == 5, "reopened actual persistence retains acknowledged edit");
            Console.WriteLine("block_receipt_production PASS valid_break_place invalid_reach rapid_same_column native_drain durable_save save_failure unchanged_revision movement_wait timeout bounded_admission capacity_retry ack_retirement reconnect reopen");
            return 0;
        }
        finally
        {
            RestorePersistencePath(previous);
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", previousPlayers);
        }
    }
}
