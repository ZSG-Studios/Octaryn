using System.Runtime.InteropServices;

namespace Octaryn.Server.World.Blocks;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeFluidConfig
{
    public uint Version, Size;
    public fixed ushort Water[8];
    public fixed ushort Lava[8];
    public ushort Stone, Reserved;
    public uint ReplaceableCount, SolidCount;
    public ushort* Replaceable;
    public ushort* Solid;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeFluidTickReport
{
    public readonly uint Version, Size, Evaluations, ApplyAttempts;
    public readonly uint Changed, Pending, Reads, RepairSamples;
    public readonly uint CapacityDeferrals, BudgetExhausted;
    public readonly ulong NowMs;
}
