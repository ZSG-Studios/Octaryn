using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 40)]
public readonly struct ServerSnapshotHeader
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 40u;

    public readonly uint Version;
    public readonly uint Size;
    public readonly uint ReplicationCount;
    public readonly uint ChangeCount;
    public readonly ulong TickId;
    public readonly ulong ReplicationIdsAddress;
    public readonly ulong ChangesAddress;

    internal ServerSnapshotHeader(
        uint replicationCount,
        uint changeCount,
        ulong tickId,
        ulong replicationIdsAddress,
        ulong changesAddress)
    {
        Version = VersionValue;
        Size = SizeValue;
        ReplicationCount = replicationCount;
        ChangeCount = changeCount;
        TickId = tickId;
        ReplicationIdsAddress = replicationIdsAddress;
        ChangesAddress = changesAddress;
    }
}
