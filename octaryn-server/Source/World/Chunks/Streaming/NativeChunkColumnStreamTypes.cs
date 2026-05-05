using System.Runtime.InteropServices;

namespace Octaryn.Server.World.Chunks;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkWindowEvent(uint kind, int chunkX, int chunkZ)
{
    public readonly uint Kind = kind;
    public readonly int ChunkX = chunkX;
    public readonly int ChunkZ = chunkZ;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamColumn(
    int chunkX,
    int chunkZ,
    int originX,
    int originZ,
    uint blockOffset,
    uint blockCount)
{
    public readonly int ChunkX = chunkX;
    public readonly int ChunkZ = chunkZ;
    public readonly int OriginX = originX;
    public readonly int OriginZ = originZ;
    public readonly uint BlockOffset = blockOffset;
    public readonly uint BlockCount = blockCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamBlock(int x, int y, int z, ushort block)
{
    public readonly int X = x;
    public readonly int Y = y;
    public readonly int Z = z;
    public readonly ushort Block = block;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamCounts(uint eventCount, uint columnCount, uint blockCount)
{
    public readonly uint EventCount = eventCount;
    public readonly uint ColumnCount = columnCount;
    public readonly uint BlockCount = blockCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamSnapshotRequest(
    IntPtr streamPath,
    ulong epoch,
    int centerChunkX,
    int centerChunkZ,
    uint radius,
    uint hasPreviousWindow,
    int previousCenterChunkX,
    int previousCenterChunkZ,
    uint previousRadius,
    uint metadataOnly,
    ulong worldSeed,
    ulong worldTimeDayIndex,
    uint worldTimeSecondOfDay,
    double worldTimeTotalSeconds,
    float worldTimeDayFraction,
    float playerX,
    float playerY,
    float playerZ,
    float playerPitch,
    float playerYaw,
    float playerVelocityX,
    float playerVelocityY,
    float playerVelocityZ,
    uint playerControlMode,
    uint playerOnGround)
{
    public readonly IntPtr StreamPath = streamPath;
    public readonly ulong Epoch = epoch;
    public readonly int CenterChunkX = centerChunkX;
    public readonly int CenterChunkZ = centerChunkZ;
    public readonly uint Radius = radius;
    public readonly uint HasPreviousWindow = hasPreviousWindow;
    public readonly int PreviousCenterChunkX = previousCenterChunkX;
    public readonly int PreviousCenterChunkZ = previousCenterChunkZ;
    public readonly uint PreviousRadius = previousRadius;
    public readonly uint MetadataOnly = metadataOnly;
    public readonly ulong WorldSeed = worldSeed;
    public readonly ulong WorldTimeDayIndex = worldTimeDayIndex;
    public readonly uint WorldTimeSecondOfDay = worldTimeSecondOfDay;
    public readonly double WorldTimeTotalSeconds = worldTimeTotalSeconds;
    public readonly float WorldTimeDayFraction = worldTimeDayFraction;
    public readonly float PlayerX = playerX;
    public readonly float PlayerY = playerY;
    public readonly float PlayerZ = playerZ;
    public readonly float PlayerPitch = playerPitch;
    public readonly float PlayerYaw = playerYaw;
    public readonly float PlayerVelocityX = playerVelocityX;
    public readonly float PlayerVelocityY = playerVelocityY;
    public readonly float PlayerVelocityZ = playerVelocityZ;
    public readonly uint PlayerControlMode = playerControlMode;
    public readonly uint PlayerOnGround = playerOnGround;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamSnapshotResult(
    NativeChunkStreamCounts counts,
    uint loadCount,
    uint preserveCount,
    uint unloadCount)
{
    public readonly NativeChunkStreamCounts Counts = counts;
    public readonly uint LoadCount = loadCount;
    public readonly uint PreserveCount = preserveCount;
    public readonly uint UnloadCount = unloadCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkStreamWriteDecision(uint usePreviousWindow, uint shouldWrite)
{
    public readonly uint UsePreviousWindow = usePreviousWindow;
    public readonly uint ShouldWrite = shouldWrite;
}
