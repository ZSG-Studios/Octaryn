using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 40)]
public readonly struct ReplicationChange
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 40u;

    public readonly uint Version;
    public readonly uint Size;
    public readonly uint ChangeKind;
    public readonly uint Flags;
    public readonly ulong ReplicationId;
    public readonly ulong Payload0;
    public readonly ulong Payload1;

    internal ReplicationChange(
        uint changeKind,
        ulong replicationId,
        ulong payload0,
        ulong payload1)
    {
        Version = VersionValue;
        Size = SizeValue;
        ChangeKind = changeKind;
        Flags = 0;
        ReplicationId = replicationId;
        Payload0 = payload0;
        Payload1 = payload1;
    }
}
