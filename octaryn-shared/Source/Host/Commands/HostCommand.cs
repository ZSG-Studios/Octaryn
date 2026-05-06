using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 96)]
internal struct HostCommand
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 96u;
    public const uint CriticalFlag = 1u;
    public const uint ClientInteractionFlag = 1u << 1;

    public uint Version;
    public uint Size;
    public HostCommandKind Kind;
    public uint Flags;
    public ulong RequestId;
    public ulong TargetId;
    public int A;
    public int B;
    public int C;
    public int D;
    public float X;
    public float Y;
    public float Z;
    public float W;
    public float X2;
    public float Y2;
    public float Z2;
    public float W2;
    public ulong Payload0;
    public ulong Payload1;
}
