using System.Runtime.InteropServices;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Host;

internal static unsafe class NativeHostPolicyLibrary
{
    private const string LibraryName = "octaryn_server_host";

    private static readonly delegate* unmanaged[Cdecl]<NativeHostStartupPolicy> GetStartupPolicyNative;
    private static readonly delegate* unmanaged[Cdecl]<HostFrameSnapshot*, void> CreateStartupFrameNative;
    private static readonly delegate* unmanaged[Cdecl]<uint, void> SleepLiveStreamIntervalNative;

    static NativeHostPolicyLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        GetStartupPolicyNative = (delegate* unmanaged[Cdecl]<NativeHostStartupPolicy>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_get_startup_policy");
        CreateStartupFrameNative = (delegate* unmanaged[Cdecl]<HostFrameSnapshot*, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_create_startup_frame");
        SleepLiveStreamIntervalNative = (delegate* unmanaged[Cdecl]<uint, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_sleep_live_stream_interval");
    }

    public static NativeHostStartupPolicy GetStartupPolicy()
    {
        return GetStartupPolicyNative();
    }

    public static HostFrameSnapshot CreateStartupFrame()
    {
        HostFrameSnapshot frame = default;
        CreateStartupFrameNative(&frame);
        return frame;
    }

    public static void SleepLiveStreamInterval(uint intervalMilliseconds)
    {
        SleepLiveStreamIntervalNative(intervalMilliseconds);
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_HOST_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeHostPolicyLibrary).Assembly.Location;
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
