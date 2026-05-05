using System.Runtime.InteropServices;

namespace Octaryn.Server.World.Generation;

internal static unsafe class NativeTerrainGenerationLibrary
{
    private const string LibraryName = "octaryn_server_terrain_generation";

    public static readonly delegate* unmanaged[Cdecl]<int, int, int, int, ushort, delegate* unmanaged[Cdecl]<void*, NativeTerrainColumnSample*, NativeTerrainColumnPlan*, int>, void*, ushort*, int> GeneratedBlock;
    public static readonly delegate* unmanaged[Cdecl]<int, int, int, ushort> EmptyWorldGeneratedBlock;

    static NativeTerrainGenerationLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        GeneratedBlock = (delegate* unmanaged[Cdecl]<int, int, int, int, ushort, delegate* unmanaged[Cdecl]<void*, NativeTerrainColumnSample*, NativeTerrainColumnPlan*, int>, void*, ushort*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_terrain_generated_block");
        EmptyWorldGeneratedBlock = (delegate* unmanaged[Cdecl]<int, int, int, ushort>)NativeLibrary.GetExport(
            library,
            "octaryn_server_empty_world_generated_block");
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeTerrainGenerationLibrary).Assembly.Location;
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
