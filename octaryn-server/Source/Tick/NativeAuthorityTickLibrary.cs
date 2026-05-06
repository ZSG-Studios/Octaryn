using System.Runtime.InteropServices;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Tick;

internal static unsafe class NativeAuthorityTickLibrary
{
    private const string LibraryName = "octaryn_server_authority_tick";

    private static readonly delegate* unmanaged[Cdecl]<
        IntPtr,
        NativeAuthorityTickCallbacks*,
        NativeScheduleRuntimeReport*,
        int> ExecuteAuthorityTick;

    static NativeAuthorityTickLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        ExecuteAuthorityTick = (delegate* unmanaged[Cdecl]<
            IntPtr,
            NativeAuthorityTickCallbacks*,
            NativeScheduleRuntimeReport*,
            int>)NativeLibrary.GetExport(library, "octaryn_server_authority_tick_execute");
    }

    public static int Execute(
        IntPtr scheduleRuntime,
        NativeAuthorityTickCallbacks* callbacks,
        NativeScheduleRuntimeReport* report)
    {
        return ExecuteAuthorityTick(scheduleRuntime, callbacks, report);
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeAuthorityTickLibrary).Assembly.Location;
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

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
internal readonly unsafe struct NativeAuthorityTickCallbacks(
    delegate* unmanaged[Cdecl]<void*, int> playerTick,
    void* playerContext,
    delegate* unmanaged[Cdecl]<void*, int> worldTimeTick,
    void* worldTimeContext)
{
    public readonly delegate* unmanaged[Cdecl]<void*, int> PlayerTick = playerTick;
    public readonly void* PlayerContext = playerContext;
    public readonly delegate* unmanaged[Cdecl]<void*, int> WorldTimeTick = worldTimeTick;
    public readonly void* WorldTimeContext = worldTimeContext;
}
