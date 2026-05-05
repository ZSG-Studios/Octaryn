using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static bool TryReadWorldBlockOverrideFile(
        string path,
        out NativePersistenceWorldBlockOverrideFile file,
        out NativePersistenceBlockEdit[] blocks)
    {
        file = default;
        blocks = [];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceWorldBlockOverrideFile* filePointer = &file)
            {
                if (s_readWorldBlockOverrideFileCount(pathPointer, filePointer) != 0 ||
                    file.BlockCount > int.MaxValue)
                {
                    return false;
                }

                blocks = new NativePersistenceBlockEdit[checked((int)file.BlockCount)];
                fixed (NativePersistenceBlockEdit* blockPointer = blocks)
                {
                    var result = s_readWorldBlockOverrideFileFill(
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

    public static void WriteWorldBlockOverrideFile(
        string path,
        NativePersistenceWorldBlockOverrideFile file,
        ReadOnlySpan<NativePersistenceBlockEdit> blocks)
    {
        if (file.BlockCount != (uint)blocks.Length)
        {
            throw new ArgumentException("World-block override block count must match the supplied block span.", nameof(blocks));
        }

        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceBlockEdit* blockPointer = blocks)
            {
                var result = s_writeWorldBlockOverrideFile(pathPointer, &file, blockPointer);
                if (result != 0)
                {
                    throw new IOException("Native world-block override write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }
}
