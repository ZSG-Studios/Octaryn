using System.Runtime.InteropServices;

namespace Octaryn.Client.WorldPresentation;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = SizeValue)]
internal readonly struct ClientPackedMeshUploadDescriptor
{
    public const uint VersionValue = 1;
    public const int SizeValue = 56;
    public const uint ClearOpaqueFacesFlag = 1u << 0;
    public const uint ClearTransparentFacesFlag = 1u << 1;
    public const uint ClearSpriteVerticesFlag = 1u << 2;

    public ClientPackedMeshUploadDescriptor(
        uint opaqueFaceCount,
        uint transparentFaceCount,
        uint spriteVertexCount,
        uint spriteIndexCount,
        ulong opaqueByteCount,
        ulong transparentByteCount,
        ulong spriteByteCount,
        uint flags)
    {
        Version = VersionValue;
        Size = SizeValue;
        OpaqueFaceCount = opaqueFaceCount;
        TransparentFaceCount = transparentFaceCount;
        SpriteVertexCount = spriteVertexCount;
        SpriteIndexCount = spriteIndexCount;
        OpaqueByteCount = opaqueByteCount;
        TransparentByteCount = transparentByteCount;
        SpriteByteCount = spriteByteCount;
        Flags = flags;
    }

    public uint Version { get; }

    public uint Size { get; }

    public uint OpaqueFaceCount { get; }

    public uint TransparentFaceCount { get; }

    public uint SpriteVertexCount { get; }

    public uint SpriteIndexCount { get; }

    public ulong OpaqueByteCount { get; }

    public ulong TransparentByteCount { get; }

    public ulong SpriteByteCount { get; }

    public uint Flags { get; }
}
