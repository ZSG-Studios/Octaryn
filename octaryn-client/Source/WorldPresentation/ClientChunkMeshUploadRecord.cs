using System.Runtime.InteropServices;

namespace Octaryn.Client.WorldPresentation;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = SizeValue)]
internal struct ClientChunkMeshUploadRecord
{
    public const uint VersionValue = 1;
    public const int SizeValue = 96;
    public const uint ClearOpaqueFacesFlag = 1u << 0;
    public const uint ClearTransparentFacesFlag = 1u << 1;
    public const uint ClearSpriteVerticesFlag = 1u << 2;
    public const uint ClearFluidBlocksFlag = 1u << 3;

    public uint Version;
    public uint Size;
    public int ChunkX;
    public int ChunkY;
    public int ChunkZ;
    public uint Flags;
    public uint OpaqueFaceCount;
    public uint TransparentFaceCount;
    public uint SpriteVertexCount;
    public uint SpriteIndexCount;
    public uint FluidBlockCount;
    public uint Reserved;
    public ulong OpaqueFaceOffset;
    public ulong TransparentFaceOffset;
    public ulong SpriteVertexOffset;
    public ulong OpaqueByteCount;
    public ulong TransparentByteCount;
    public ulong SpriteByteCount;

    public static ClientChunkMeshUploadRecord Create(
        ClientPresentationChunkKey chunk,
        ClientPackedChunkMesh mesh,
        uint opaqueFaceOffset,
        uint transparentFaceOffset,
        uint spriteVertexOffset)
    {
        var opaqueFaceCount = checked((uint)mesh.OpaqueCubeFaces.Count);
        var transparentFaceCount = checked((uint)mesh.TransparentCubeFaces.Count);
        var spriteVertexCount = checked((uint)mesh.SpriteVertices.Count);
        var fluidBlockCount = checked((uint)mesh.FluidBlocks.Count);
        var flags = 0u;
        if (opaqueFaceCount == 0)
        {
            flags |= ClearOpaqueFacesFlag;
        }

        if (transparentFaceCount == 0)
        {
            flags |= ClearTransparentFacesFlag;
        }

        if (spriteVertexCount == 0)
        {
            flags |= ClearSpriteVerticesFlag;
        }

        if (fluidBlockCount == 0)
        {
            flags |= ClearFluidBlocksFlag;
        }

        return new ClientChunkMeshUploadRecord
        {
            Version = VersionValue,
            Size = SizeValue,
            ChunkX = chunk.X,
            ChunkY = chunk.Y,
            ChunkZ = chunk.Z,
            Flags = flags,
            OpaqueFaceCount = opaqueFaceCount,
            TransparentFaceCount = transparentFaceCount,
            SpriteVertexCount = spriteVertexCount,
            SpriteIndexCount = checked((uint)(mesh.SpriteVertices.Count / 4 * 6)),
            FluidBlockCount = fluidBlockCount,
            OpaqueFaceOffset = opaqueFaceOffset,
            TransparentFaceOffset = transparentFaceOffset,
            SpriteVertexOffset = spriteVertexOffset,
            OpaqueByteCount = (ulong)mesh.OpaqueCubeFaces.Count * sizeof(ulong),
            TransparentByteCount = (ulong)mesh.TransparentCubeFaces.Count * sizeof(ulong),
            SpriteByteCount = (ulong)mesh.SpriteVertices.Count * sizeof(uint)
        };
    }
}
