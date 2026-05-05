using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static bool TryReadChunkOverrideFile(
        string path,
        out NativePersistenceChunkOverrideFile file,
        out NativePersistenceChunkOverrideBlock[] blocks)
    {
        file = default;
        blocks = [];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceChunkOverrideFile* filePointer = &file)
            {
                if (s_readChunkOverrideFileCount(pathPointer, filePointer) != 0 ||
                    file.BlockCount > int.MaxValue)
                {
                    return false;
                }

                blocks = new NativePersistenceChunkOverrideBlock[checked((int)file.BlockCount)];
                fixed (NativePersistenceChunkOverrideBlock* blockPointer = blocks)
                {
                    var result = s_readChunkOverrideFileFill(
                        pathPointer,
                        blockPointer,
                        file.BlockCount,
                        filePointer);
                    return result == 0 && file.BlockCount == (uint)blocks.Length;
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteChunkOverrideFile(
        string path,
        NativePersistenceChunkOverrideFile file,
        ReadOnlySpan<NativePersistenceChunkOverrideBlock> blocks)
    {
        if (file.BlockCount != (uint)blocks.Length)
        {
            throw new ArgumentException("Chunk-column override block count must match the supplied block span.", nameof(blocks));
        }

        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceChunkOverrideBlock* blockPointer = blocks)
            {
                var result = s_writeChunkOverrideFile(pathPointer, &file, blockPointer);
                if (result != 0)
                {
                    throw new IOException("Native chunk-column override write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }
}
