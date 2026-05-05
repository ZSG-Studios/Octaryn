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
