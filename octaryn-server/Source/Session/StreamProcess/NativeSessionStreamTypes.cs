using System.Runtime.InteropServices;

namespace Octaryn.Server.Session;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkViewIntent(
    int version,
    ulong epoch,
    int centerChunkX,
    int centerChunkZ,
    uint radius,
    uint hasPreviousWindow,
    int previousCenterChunkX,
    int previousCenterChunkZ,
    uint previousRadius)
{
    public readonly int Version = version;
    public readonly ulong Epoch = epoch;
    public readonly int CenterChunkX = centerChunkX;
    public readonly int CenterChunkZ = centerChunkZ;
    public readonly uint Radius = radius;
    public readonly uint HasPreviousWindow = hasPreviousWindow;
    public readonly int PreviousCenterChunkX = previousCenterChunkX;
    public readonly int PreviousCenterChunkZ = previousCenterChunkZ;
    public readonly uint PreviousRadius = previousRadius;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamProcessTickDecision(
    uint shouldTick,
    uint useHostOnlyTick,
    uint useDefaultFrame)
{
    public readonly uint ShouldTick = shouldTick;
    public readonly uint UseHostOnlyTick = useHostOnlyTick;
    public readonly uint UseDefaultFrame = useDefaultFrame;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamProcessWritePlan(
    uint shouldContinue,
    uint shouldWrite,
    uint usePreviousWindow,
    uint reason,
    int handleResult,
    int centerChunkX,
    int centerChunkZ,
    uint radius)
{
    public readonly uint ShouldContinue = shouldContinue;
    public readonly uint ShouldWrite = shouldWrite;
    public readonly uint UsePreviousWindow = usePreviousWindow;
    public readonly uint Reason = reason;
    public readonly int HandleResult = handleResult;
    public readonly int CenterChunkX = centerChunkX;
    public readonly int CenterChunkZ = centerChunkZ;
    public readonly uint Radius = radius;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamProcessStagePlan(
    NativeChunkStreamProcessTickDecision tick,
    NativeChunkStreamProcessWritePlan write)
{
    public readonly NativeChunkStreamProcessTickDecision Tick = tick;
    public readonly NativeChunkStreamProcessWritePlan Write = write;
}
