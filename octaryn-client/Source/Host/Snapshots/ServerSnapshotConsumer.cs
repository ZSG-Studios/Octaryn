using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.Networking;

namespace Octaryn.Client.Host;

internal sealed class ServerSnapshotConsumer(BlockPresentationStore blocks)
{
    private ulong _lastAppliedTickId;

    public unsafe int Apply(ServerSnapshotHeader* snapshotHeader)
    {
        if (snapshotHeader is null ||
            snapshotHeader->Version != ServerSnapshotHeader.VersionValue ||
            snapshotHeader->Size != ServerSnapshotHeader.SizeValue)
        {
            return -1;
        }

        if (snapshotHeader->TickId < _lastAppliedTickId)
        {
            return -3;
        }

        if (snapshotHeader->ChangeCount == 0)
        {
            _lastAppliedTickId = snapshotHeader->TickId;
            return 0;
        }

        var changes = (ReplicationChange*)snapshotHeader->ChangesAddress;
        if (changes is null)
        {
            return -1;
        }

        for (var index = 0u; index < snapshotHeader->ChangeCount; index++)
        {
            ref readonly var change = ref changes[index];
            if (!BlockReplicationChange.TryRead(in change, out var blockChange))
            {
                return -2;
            }

            _ = blocks.Apply(blockChange.Position, blockChange.Block);
        }

        _lastAppliedTickId = snapshotHeader->TickId;
        return 0;
    }
}
