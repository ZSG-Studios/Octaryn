using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 40)]
public readonly struct NetworkMessageHeader
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 40u;

    public readonly uint Version;
    public readonly uint Size;
    public readonly uint MessageKind;
    public readonly uint Flags;
    public readonly ulong TickId;
    public readonly ulong ReplicationId;
    public readonly ulong PayloadBytes;
}
