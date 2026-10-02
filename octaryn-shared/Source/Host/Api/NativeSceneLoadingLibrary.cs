using System.Runtime.InteropServices;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal unsafe sealed class NativeSceneLoadingLibrary
{
    private static readonly object s_sync = new();
    private static NativeSceneLoadingLibrary? s_library;
    internal readonly delegate* unmanaged[Cdecl]<byte*, byte*, nint, nint> Create;
    internal readonly delegate* unmanaged[Cdecl]<nint, byte*, HostSceneTicketNative*, int> Begin;
    internal readonly delegate* unmanaged[Cdecl]<nint, HostSceneTicketNative*, HostSceneProgressNative*, int> Query;
    internal readonly delegate* unmanaged[Cdecl]<nint, HostSceneTicketNative*, int> Cancel, Release;
    internal readonly delegate* unmanaged[Cdecl]<nint, void> Destroy;
    internal readonly delegate* unmanaged[Cdecl]<nint, HostSceneTicketNative*, byte*, uint, int> Error;

    private NativeSceneLoadingLibrary(nint library)
    {
        Create = (delegate* unmanaged[Cdecl]<byte*, byte*, nint, nint>)NativeLibrary.GetExport(library, "octaryn_scene_loading_create");
        Begin = (delegate* unmanaged[Cdecl]<nint, byte*, HostSceneTicketNative*, int>)NativeLibrary.GetExport(library, "octaryn_scene_loading_begin");
        Query = (delegate* unmanaged[Cdecl]<nint, HostSceneTicketNative*, HostSceneProgressNative*, int>)NativeLibrary.GetExport(library, "octaryn_scene_loading_query");
        Cancel = (delegate* unmanaged[Cdecl]<nint, HostSceneTicketNative*, int>)NativeLibrary.GetExport(library, "octaryn_scene_loading_cancel");
        Release = (delegate* unmanaged[Cdecl]<nint, HostSceneTicketNative*, int>)NativeLibrary.GetExport(library, "octaryn_scene_loading_release");
        Destroy = (delegate* unmanaged[Cdecl]<nint, void>)NativeLibrary.GetExport(library, "octaryn_scene_loading_destroy");
        Error = (delegate* unmanaged[Cdecl]<nint, HostSceneTicketNative*, byte*, uint, int>)NativeLibrary.GetExport(library, "octaryn_scene_loading_error");
    }

    internal static NativeSceneLoadingLibrary? TryLoad()
    {
        lock (s_sync)
        {
            if (s_library is not null) return s_library;
            var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SCENE_LOADING_LIBRARY");
            var file = OperatingSystem.IsWindows() ? "octaryn_scene_loading.dll" :
                OperatingSystem.IsMacOS() ? "liboctaryn_scene_loading.dylib" : "liboctaryn_scene_loading.so";
            var candidates = string.IsNullOrWhiteSpace(explicitPath)
                ? new[] { Path.Combine(GameModuleBundle.ResolveHostDirectory(Path.GetDirectoryName(typeof(NativeSceneLoadingLibrary).Assembly.Location)), file),
                    Path.Combine(GameModuleBundle.ResolveHostDirectory(AppContext.BaseDirectory), file) }
                : new[] { Path.GetFullPath(explicitPath) };
            foreach (var path in candidates)
            {
                if (!File.Exists(path) || !NativeLibrary.TryLoad(path, out var library)) continue;
                try
                {
                    s_library = new NativeSceneLoadingLibrary(library);
                    return s_library; // Function pointers retain a process-lifetime library lease.
                }
                catch (EntryPointNotFoundException)
                {
                    NativeLibrary.Free(library);
                }
            }
            return null;
        }
    }
}
