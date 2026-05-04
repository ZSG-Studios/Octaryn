using Octaryn.Client.ClientHost;
using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

internal static class SnapshotConsumerValidation
{
    public static unsafe void ValidateTickOrder()
    {
        var store = new BlockPresentationStore();
        var consumer = new ClientServerSnapshotConsumer(store);
        var changes = stackalloc ReplicationChange[1];
        changes[0] = new BlockReplicationChange(new BlockPosition(2, 3, 4), new BlockId(5))
            .ToReplicationChange(10);

        var current = new ServerSnapshotHeader(0, 1, 10, 0, (ulong)changes);
        ProbeAssertions.Require(consumer.Apply(&current) == 0, "consumer accepts current snapshot");
        ProbeAssertions.Require(store.GetBlock(new BlockPosition(2, 3, 4)).Value == 5, "consumer applies current snapshot");

        var stale = new ServerSnapshotHeader(0, 1, 9, 0, (ulong)changes);
        ProbeAssertions.Require(consumer.Apply(&stale) == -3, "consumer rejects stale snapshot");

        var emptyNewer = new ServerSnapshotHeader(0, 0, 12, 0, 0);
        ProbeAssertions.Require(consumer.Apply(&emptyNewer) == 0, "consumer accepts empty newer snapshot");

        var staleAfterEmpty = new ServerSnapshotHeader(0, 1, 11, 0, (ulong)changes);
        ProbeAssertions.Require(consumer.Apply(&staleAfterEmpty) == -3, "consumer rejects change older than empty snapshot");

        var emptyStale = new ServerSnapshotHeader(0, 0, 11, 0, 0);
        ProbeAssertions.Require(consumer.Apply(&emptyStale) == -3, "consumer rejects empty stale snapshot");
    }
}
