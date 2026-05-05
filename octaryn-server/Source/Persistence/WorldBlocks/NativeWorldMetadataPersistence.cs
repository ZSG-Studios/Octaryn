using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static bool TryReadWorldMetadataFile(string path, out NativePersistenceWorldMetadata metadata)
    {
        metadata = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceWorldMetadata* metadataPointer = &metadata)
            {
                return s_readWorldMetadataFile(pathPointer, metadataPointer) == 0;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteWorldMetadataFile(string path, NativePersistenceWorldMetadata metadata)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var result = s_writeWorldMetadataFile(pathPointer, &metadata);
            if (result != 0)
            {
                throw new IOException("Native world metadata save write failed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }
}
