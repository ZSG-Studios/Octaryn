using Octaryn.Server.Modules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static unsafe void ValidateReplicationBackpressure()
    {
        const int capacity = 8192;
        var previousPath = UseProbePersistenceFile("replication-backpressure");
        var path = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH")!;
        try
        {
            using (var authority = new ModuleActivator(new BlockEditRegistration()))
            {
                Require(authority.Activate(new RejectingCommandSink()) == 0, "backpressure activate");
                Require(authority.SubmitClientCommands(PressureCommands(0, 4096)) == 0, "first pressure batch");
                authority.Tick(Frame(1)); // The module adds one different edit after client commands.
                Require(authority.SubmitClientCommands(PressureCommands(4096, 4095)) == 0, "second pressure batch");
                authority.Tick(Frame(2));
                Require(authority.PendingBlockChangeCount == capacity, "bounded replication capacity");
                var revision = authority.BlockRevision;
                var persisted = File.ReadAllBytes(path);

                Require(authority.SubmitClientCommands(PressureCommands(8191, 2)) == 0, "deferred client batch accepted");
                authority.Tick(Frame(3));
                Require(authority.PendingClientBlockCommandCount == 2, "full queue retains client FIFO");
                Require(authority.PendingBlockChangeCount == capacity, "full queue does not grow");
                Require(authority.BlockRevision == revision, "deferred edit does not advance revision");
                Require(authority.GetBlock(new BlockPosition(8291, 60, 100)) == BlockId.Air, "deferred block unmodified");
                Require(File.ReadAllBytes(path).AsSpan().SequenceEqual(persisted), "deferred edits do not rewrite persistence");

                var changes = new ReplicationChange[capacity];
                fixed (ReplicationChange* output = changes)
                {
                    var header = new ServerSnapshotHeader(0, capacity - 1, 0, 0, (ulong)output);
                    Require(authority.DrainServerSnapshots(&header) == -1, "undersized output preserves queue");
                    Require(authority.PendingBlockChangeCount == capacity, "undersized output loses no events");
                    header = new ServerSnapshotHeader(0, capacity, 0, 0, (ulong)output);
                    Require(authority.DrainServerSnapshots(&header) == 0 && header.ChangeCount == capacity,
                        "full replication drain recovers capacity");
                    for (var index = 0; index < capacity; index++)
                    {
                        if (index == 4096)
                        {
                            Require(UnpackLow(changes[index].Payload0) == 8 && UnpackHigh(changes[index].Payload0) == 9 &&
                                UnpackLow(changes[index].Payload1) == 10 && (ushort)(changes[index].Payload1 >> 32) == 5,
                                "module event retains position in FIFO");
                            continue;
                        }
                        var commandIndex = index < 4096 ? index : index - 1;
                        RequirePressureChange(changes[index], commandIndex);
                    }

                    authority.Tick(Frame(4));
                    Require(authority.PendingClientBlockCommandCount == 0, "deferred commands resume");
                    Require(authority.PendingBlockChangeCount == 2 && authority.BlockRevision == revision + 2,
                        "retry commits each deferred change exactly once");
                    header = new ServerSnapshotHeader(0, capacity, 0, 0, (ulong)output);
                    Require(authority.DrainServerSnapshots(&header) == 0 && header.ChangeCount == 2, "retry output count");
                    RequirePressureChange(changes[0], 8191);
                    RequirePressureChange(changes[1], 8192);
                    authority.Tick(Frame(5));
                    Require(authority.PendingBlockChangeCount == 0 && authority.BlockRevision == revision + 2,
                        "no duplicated retry or unchanged module event");
                }
            }

            using var reloaded = new ModuleActivator(new BlockEditRegistration());
            Require(reloaded.GetBlock(new BlockPosition(8291, 60, 100)).Value == 5 &&
                reloaded.GetBlock(new BlockPosition(8292, 60, 100)).Value == 5, "retried edits survive on-disk reload");
        }
        finally
        {
            RestorePersistencePath(previousPath);
        }
        Console.WriteLine("replication_backpressure=passed capacity=8192 fifo=passed revision=passed persistence_retry=passed");
    }

    private static HostCommand[] PressureCommands(int first, int count)
    {
        var commands = new HostCommand[count];
        for (var index = 0; index < count; index++)
        {
            commands[index] = new HostCommand
            {
                Version = HostCommand.VersionValue, Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock, A = first + index + 100, B = 60, C = 100, D = 5
            };
        }
        return commands;
    }

    private static void RequirePressureChange(ReplicationChange change, int index)
    {
        Require(UnpackLow(change.Payload0) == index + 100 && UnpackHigh(change.Payload0) == 60 &&
            UnpackLow(change.Payload1) == 100 && (ushort)(change.Payload1 >> 32) == 5,
            "complete ordered replication payload");
    }
}
