using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, uint*, int> s_worldGenerationRevision =
        (delegate* unmanaged[Cdecl]<IntPtr, uint*, int>)NativeLibrary.GetExport(
            NativeLibrary.Load(ResolveLibraryPath()), "octaryn_server_persistence_world_generation_revision");

    public static uint WorldGenerationRevisionForRoot(string root)
    {
        var pointer = Marshal.StringToCoTaskMemUTF8(root);
        try
        {
            uint revision = 0;
            var result = s_worldGenerationRevision(pointer, &revision);
            if (result != 0) throw new IOException($"Cannot read world generator revision for '{root}' (code {result}).");
            return revision;
        }
        finally { Marshal.FreeCoTaskMem(pointer); }
    }

    public static void EnsureWorldGeneration()
    {
        EnsureWorldGeneration(WorldRootPathFromEnvironment(), WorldBlockOverridePathFromEnvironment(),
            PlayerDirectoryPathFromEnvironment(), 0u);
    }

    public static void EnsureWorldGenerationForRoot(string worldRoot)
    {
        EnsureWorldGeneration(worldRoot, WorldBlockOverridePathForRoot(worldRoot), worldRoot, 0u);
    }

    private static void EnsureWorldGeneration(string root, string blocksPath, string playersPath, uint mode)
    {
        var rootPointer = Marshal.StringToCoTaskMemUTF8(root);
        var blocksPointer = Marshal.StringToCoTaskMemUTF8(blocksPath);
        var playersPointer = Marshal.StringToCoTaskMemUTF8(playersPath);
        try
        {
            var result = s_ensureWorldGeneration(rootPointer, blocksPointer, playersPointer, mode);
            if (result != 0)
            {
                throw new IOException($"World generator identity check failed for '{root}' (code {result}). " +
                    "Only the vegetation terrain generator (revision 3, mode 0) is supported. " +
                    "Older, unversioned, flat, and empty worlds cannot be loaded. Choose a new world directory. " +
                    "Existing save data was not changed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(rootPointer);
            Marshal.FreeCoTaskMem(blocksPointer);
            Marshal.FreeCoTaskMem(playersPointer);
        }
    }
}
