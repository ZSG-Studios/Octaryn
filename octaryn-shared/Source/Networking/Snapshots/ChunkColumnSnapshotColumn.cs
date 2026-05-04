using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
public readonly struct ChunkColumnSnapshotColumn
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 32u;

    public readonly uint Version;
    public readonly uint Size;
    public readonly int ChunkX;
    public readonly int ChunkZ;
    public readonly int OriginX;
    public readonly int OriginZ;
    public readonly uint BlockOffset;
    public readonly uint BlockCount;

    internal ChunkColumnSnapshotColumn(
        int chunkX,
        int chunkZ,
        int originX,
        int originZ,
        uint blockOffset,
        uint blockCount)
    {
        Version = VersionValue;
        Size = SizeValue;
        ChunkX = chunkX;
        ChunkZ = chunkZ;
        OriginX = originX;
        OriginZ = originZ;
        BlockOffset = blockOffset;
        BlockCount = blockCount;
    }
}
