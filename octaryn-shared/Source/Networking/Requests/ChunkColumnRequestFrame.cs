using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 56)]
public readonly struct ChunkColumnRequestFrame
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 56u;

    public readonly uint Version;
    public readonly uint Size;
    public readonly int CenterChunkX;
    public readonly int CenterChunkZ;
    public readonly uint Radius;
    public readonly uint ColumnCapacity;
    public readonly uint BlockCapacity;
    public readonly uint ColumnCount;
    public readonly uint BlockCount;
    public readonly uint Status;
    public readonly ulong ColumnsAddress;
    public readonly ulong BlocksAddress;

    internal ChunkColumnRequestFrame(
        int centerChunkX,
        int centerChunkZ,
        uint radius,
        uint columnCapacity,
        uint blockCapacity,
        uint columnCount,
        uint blockCount,
        uint status,
        ulong columnsAddress,
        ulong blocksAddress)
    {
        Version = VersionValue;
        Size = SizeValue;
        CenterChunkX = centerChunkX;
        CenterChunkZ = centerChunkZ;
        Radius = radius;
        ColumnCapacity = columnCapacity;
        BlockCapacity = blockCapacity;
        ColumnCount = columnCount;
        BlockCount = blockCount;
        Status = status;
        ColumnsAddress = columnsAddress;
        BlocksAddress = blocksAddress;
    }
}
