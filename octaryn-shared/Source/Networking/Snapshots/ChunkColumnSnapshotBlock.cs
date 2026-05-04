using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
public readonly struct ChunkColumnSnapshotBlock
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 24u;

    public readonly uint Version;
    public readonly uint Size;
    public readonly int X;
    public readonly int Y;
    public readonly int Z;
    public readonly ushort Block;
    public readonly ushort Reserved;

    internal ChunkColumnSnapshotBlock(int x, int y, int z, ushort block)
    {
        Version = VersionValue;
        Size = SizeValue;
        X = x;
        Y = y;
        Z = z;
        Block = block;
        Reserved = 0;
    }
}
