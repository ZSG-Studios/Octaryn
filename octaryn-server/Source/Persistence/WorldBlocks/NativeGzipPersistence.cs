using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static bool TryReadGzipFile(string path, out byte[] payload)
    {
        payload = [];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            ulong size = 0;
            if (s_readGzipFileCount(pathPointer, &size) != 0 || size > int.MaxValue)
            {
                return false;
            }

            payload = new byte[(int)size];
            fixed (byte* payloadPointer = payload)
            {
                ulong written = 0;
                return s_readGzipFileFill(pathPointer, payloadPointer, size, &written) == 0 &&
                    written == size;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteGzipFile(string path, ReadOnlySpan<byte> payload)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (byte* payloadPointer = payload)
            {
                var result = s_writeGzipFile(pathPointer, payloadPointer, (ulong)payload.Length);
                if (result != 0)
                {
                    throw new IOException("Native gzip save export write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }
}
