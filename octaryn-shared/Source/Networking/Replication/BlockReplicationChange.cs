using Octaryn.Shared.World;

namespace Octaryn.Shared.Networking;

internal readonly record struct BlockReplicationChange(BlockPosition Position, BlockId Block)
{
    public const uint ChangeKind = 1u;

    public ReplicationChange ToReplicationChange(ulong tickId)
    {
        return new ReplicationChange(
            ChangeKind,
            tickId,
            PackSignedPair(Position.X, Position.Y),
            PackBlock(Position.Z, Block));
    }

    public static bool TryRead(in ReplicationChange change, out BlockReplicationChange blockChange)
    {
        blockChange = default;
        if (change.Version != ReplicationChange.VersionValue ||
            change.Size != ReplicationChange.SizeValue ||
            change.ChangeKind != ChangeKind)
        {
            return false;
        }

        blockChange = new BlockReplicationChange(
            new BlockPosition(UnpackLow(change.Payload0), UnpackHigh(change.Payload0), UnpackLow(change.Payload1)),
            new BlockId((ushort)(change.Payload1 >> 32)));
        return true;
    }

    private static ulong PackSignedPair(int a, int b)
    {
        return (uint)a | ((ulong)(uint)b << 32);
    }

    private static ulong PackBlock(int z, BlockId block)
    {
        return (uint)z | ((ulong)block.Value << 32);
    }

    private static int UnpackLow(ulong value)
    {
        return unchecked((int)(uint)value);
    }

    private static int UnpackHigh(ulong value)
    {
        return unchecked((int)(uint)(value >> 32));
    }
}
