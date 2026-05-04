using Octaryn.Server;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static unsafe void ValidateSnapshotDrain()
    {
        var previousPath = UseProbePersistenceFile("snapshot-drain");
        try
        {
            using var activator = new ServerModuleActivator(new BlockEditRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "snapshot drain activator");
            Require(activator.DrainServerSnapshots(null) == -1, "server snapshot drain rejects invalid header");

            var command = new HostCommand
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock,
                A = -4,
                B = 5,
                C = 6,
                D = 7
            };

            Require(activator.SubmitClientCommands(&command, 1) == 0, "snapshot drain submitted block edit");
            var frame = new HostFrameSnapshot(
                new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
                new HostFrameTimingSnapshot(
                    HostFrameTimingSnapshot.VersionValue,
                    HostFrameTimingSnapshot.SizeValue,
                    frameIndex: 3,
                    deltaSeconds: 1.0 / 60.0));
            activator.Tick(in frame);

            Require(activator.PendingBlockChangeCount == 2, "server snapshot drain reports block change count");

            var changes = new ReplicationChange[4];
            fixed (ReplicationChange* changesPointer = changes)
            {
                var header = new ServerSnapshotHeader(
                    replicationCount: 0,
                    changeCount: (uint)changes.Length,
                    tickId: 0,
                    replicationIdsAddress: 0,
                    changesAddress: (ulong)changesPointer);
                Require(activator.DrainServerSnapshots(&header) == 0, "server snapshot drain includes submitted block edit");
                Require(header.ChangeCount == 2, "server snapshot drain output count");
                Require(header.ReplicationCount == 0, "server snapshot drain no replication ids");
                Require(changes[0].ChangeKind == ServerBlockChangeQueue.BlockEditChangeKind, "server snapshot drain block change kind");
                Require(UnpackLow(changes[0].Payload0) == -4, "server snapshot drain first x");
                Require(UnpackHigh(changes[0].Payload0) == 5, "server snapshot drain first y");
                Require(UnpackLow(changes[0].Payload1) == 6, "server snapshot drain first z");
                Require((ushort)(changes[0].Payload1 >> 32) == 7, "server snapshot drain first block");
                Require(changes[1].ChangeKind == ServerBlockChangeQueue.BlockEditChangeKind, "server snapshot drain module block change kind");
                Require(activator.PendingBlockChangeCount == 0, "server snapshot drain clears pending block changes");

                Require(activator.DrainServerSnapshots(&header) == 0, "server snapshot drain empty");
                Require(header.ChangeCount == 0, "server snapshot drain empty count");
            }
        }
        finally
        {
            RestorePersistencePath(previousPath);
        }
    }

    private static unsafe void ValidateActivatorPersistenceLifecycle()
    {
        var root = ResetProbeDirectory("activator-persistence");
        var path = Path.Combine(root, "world_blocks.json");
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        SetPersistencePath(path);

        try
        {
            using (var activator = new ServerModuleActivator(new BlockEditRegistration()))
            {
                Require(activator.Activate(new RejectingCommandSink()) == 0, "persistence activator activation");
                activator.Tick(Frame(10));
            }

            Require(File.Exists(path), "persistence file created on dispose");

            using (var loaded = new ServerModuleActivator(new BlockEditRegistration()))
            {
                Require(loaded.GetBlock(new BlockPosition(8, 9, 10)).Value == 5, "persistence loaded module edit");
            }

            WorldBlockOverrideFile.Save(path, new WorldBlockOverrideFile
            {
                Blocks =
                [
                    new WorldBlockOverrideRecord(9, 0, 9, 29),
                    new WorldBlockOverrideRecord(9, 1, 9, 22)
                ]
            });

            using (var cascade = new ServerModuleActivator(new BlockEditRegistration()))
            {
                Require(cascade.GetBlock(new BlockPosition(9, 0, 9)).Value == 29, "persistence loaded support block");
                Require(cascade.GetBlock(new BlockPosition(9, 1, 9)).Value == 22, "persistence loaded supported block");

                var command = new HostCommand
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    A = 9,
                    B = 0,
                    C = 9,
                    D = 0
                };
                Require(cascade.SubmitClientCommands(&command, 1) == 0, "persistence cascade break submitted");
                Require(cascade.Activate(new RejectingCommandSink()) == 0, "persistence cascade activator activation");
                cascade.Tick(Frame(11));
            }

            Require(WorldBlockOverrideFile.TryLoad(path, out var saved), "persistence cascade file reload");
            var savedStore = new ServerBlockStore();
            savedStore.Load(saved.ToEdits());
            Require(savedStore.GetBlock(new BlockPosition(9, 0, 9)) == BlockId.Air, "persistence saved removed support");
            Require(savedStore.GetBlock(new BlockPosition(9, 1, 9)) == BlockId.Air, "persistence saved cascade removal");
        }
        finally
        {
            RestorePersistencePath(previousPath);
        }
    }
}
