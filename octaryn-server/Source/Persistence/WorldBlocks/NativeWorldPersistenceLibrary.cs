using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe class NativeWorldPersistenceLibrary
{
    private const string LibraryName = "octaryn_server_world_persistence";

    public static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> PlanChunkColumnsCount;
    public static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> PlanChunkColumnsFill;

    static NativeWorldPersistenceLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        PlanChunkColumnsCount = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_count");
        PlanChunkColumnsFill = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_fill");
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeWorldPersistenceLibrary).Assembly.Location;
        if (!string.IsNullOrWhiteSpace(assemblyPath))
        {
            var assemblyLibraryPath = Path.Combine(Path.GetDirectoryName(assemblyPath) ?? string.Empty, fileName);
            if (File.Exists(assemblyLibraryPath))
            {
                return assemblyLibraryPath;
            }
        }

        var bundledPath = Path.Combine(AppContext.BaseDirectory, fileName);
        return File.Exists(bundledPath) ? bundledPath : LibraryName;
    }
}
