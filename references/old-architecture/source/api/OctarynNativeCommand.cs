using System.Runtime.InteropServices;

namespace Octaryn.Engine.Api;

[StructLayout(LayoutKind.Sequential)]
public struct OctarynNativeCommand
{
    public const uint CriticalFlag = 1u;

    public OctarynNativeCommandKind Kind;
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

    public static OctarynNativeCommand SetBlock(int x, int y, int z, int block)
    {
        return new OctarynNativeCommand
        {
            Kind = OctarynNativeCommandKind.SetBlock,
            Flags = CriticalFlag,
            A = x,
            B = y,
            C = z,
            D = block
        };
    }
}
