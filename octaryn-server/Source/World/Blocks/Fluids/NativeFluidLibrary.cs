using System.Runtime.InteropServices;

namespace Octaryn.Server.World.Blocks;

internal static unsafe class NativeFluidLibrary
{
    private const string LibraryName = "octaryn_server_block_store";
    public static readonly delegate* unmanaged[Cdecl]<NativeFluidConfig*, IntPtr> Create;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> Destroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, int> SetRegion;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, uint, int> Notify;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, IntPtr, double,
        delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>,
        delegate* unmanaged[Cdecl]<void*, ushort, uint>,
        delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>,
        delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>,
        void*, NativeFluidTickReport*, int> Tick;

    static NativeFluidLibrary()
    {
        if (sizeof(NativeFluidConfig) != 72 || sizeof(NativeFluidTickReport) != 48)
            throw new PlatformNotSupportedException("Native fluid ABI requires matching 64-bit config/report layouts.");
        var library = NativeLibrary.Load(ResolveLibraryPath());
        Create = (delegate* unmanaged[Cdecl]<NativeFluidConfig*, IntPtr>)NativeLibrary.GetExport(library, "octaryn_server_fluid_create");
        Destroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)NativeLibrary.GetExport(library, "octaryn_server_fluid_destroy");
        SetRegion = (delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, int>)NativeLibrary.GetExport(library, "octaryn_server_fluid_set_region");
        Notify = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, uint, int>)NativeLibrary.GetExport(library, "octaryn_server_fluid_notify");
        Tick = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, IntPtr, double,
            delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>,
            delegate* unmanaged[Cdecl]<void*, ushort, uint>,
            delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>,
            delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>,
            void*, NativeFluidTickReport*, int>)NativeLibrary.GetExport(library, "octaryn_server_fluid_tick");
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_BLOCK_STORE_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath)) return explicitPath;
        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? $"lib{LibraryName}.dylib" : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeFluidLibrary).Assembly.Location;
        if (!string.IsNullOrWhiteSpace(assemblyPath))
        {
            var adjacent = Path.Combine(Path.GetDirectoryName(assemblyPath) ?? string.Empty, fileName);
            if (File.Exists(adjacent)) return adjacent;
        }
        var bundled = Path.Combine(AppContext.BaseDirectory, fileName);
        return File.Exists(bundled) ? bundled : LibraryName;
    }
}
