using System.Runtime.InteropServices;

namespace Octaryn.Server.World.Time;

internal static unsafe class NativeWorldTimeLibrary
{
    private const string LibraryName = "octaryn_server_world_time";

    public static readonly delegate* unmanaged[Cdecl]<IntPtr> ClockCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> ClockDestroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeConfig*, void> ClockReset;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, double, void> ClockAdvance;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeSnapshot> ClockSnapshot;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeBlob> ClockWriteBlob;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeConfig*, NativeWorldTimeBlob*, uint> ClockReadBlob;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> ClockDayIndex;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, double> ClockSecondsOfDay;

    static NativeWorldTimeLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        ClockCreate = (delegate* unmanaged[Cdecl]<IntPtr>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_create");
        ClockDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_destroy");
        ClockReset = (delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeConfig*, void>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_reset");
        ClockAdvance = (delegate* unmanaged[Cdecl]<IntPtr, double, void>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_advance");
        ClockSnapshot = (delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeSnapshot>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_snapshot");
        ClockWriteBlob = (delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeBlob>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_write_blob");
        ClockReadBlob = (delegate* unmanaged[Cdecl]<IntPtr, NativeWorldTimeConfig*, NativeWorldTimeBlob*, uint>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_read_blob");
        ClockDayIndex = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_day_index");
        ClockSecondsOfDay = (delegate* unmanaged[Cdecl]<IntPtr, double>)NativeLibrary.GetExport(library, "octaryn_server_world_time_clock_seconds_of_day");
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_TIME_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeWorldTimeLibrary).Assembly.Location;
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
