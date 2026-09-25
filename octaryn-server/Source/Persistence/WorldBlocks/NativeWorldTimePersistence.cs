using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static bool TryReadWorldTimeFile(string path, out NativePersistenceWorldTimeState state)
    {
        state = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceWorldTimeState* statePointer = &state)
            {
                return s_readWorldTimeFile(pathPointer, statePointer) == 0;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteWorldTimeFile(string path, NativePersistenceWorldTimeState state)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var result = s_writeWorldTimeFile(pathPointer, &state);
            if (result != 0)
            {
                throw new IOException("Native world time save write failed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }
}
