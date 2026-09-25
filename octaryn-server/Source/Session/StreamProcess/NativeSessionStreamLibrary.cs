using System.Runtime.InteropServices;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Session;

// Session stream process library: view-intent gating, tick staging and the
// write tracker for the live dedicated-server session loop.
internal static unsafe class NativeSessionStreamLibrary
{
    private const string LibraryName = "octaryn_server_session_stream";

    public static readonly delegate* unmanaged[Cdecl]<IntPtr> WriteTrackerCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> WriteTrackerDestroy;
    public static readonly delegate* unmanaged[Cdecl]<byte*, uint, NativeChunkViewIntent*, NativeChunkStreamProcessWritePlan*, int> ReadProcessIntent;
    public static readonly delegate* unmanaged[Cdecl]<uint, int, byte*> ProcessWriteReasonName;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeChunkViewIntent*, uint, uint, uint, NativeChunkStreamProcessStagePlan*, int> PlanProcessStage;
    public static readonly delegate* unmanaged[Cdecl]<NativeChunkStreamProcessTickDecision*, HostFrameSnapshot*, delegate* unmanaged[Cdecl]<void*, HostFrameSnapshot*, int>, delegate* unmanaged[Cdecl]<void*, HostFrameSnapshot*, int>, void*, int> ExecuteProcessTick;
    public static readonly delegate* unmanaged[Cdecl]<HostFrameSnapshot*, int> CreateProcessFrame;

    static NativeSessionStreamLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        WriteTrackerCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_chunk_stream_write_tracker_create");
        WriteTrackerDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_chunk_stream_write_tracker_destroy");
        ReadProcessIntent = (delegate* unmanaged[Cdecl]<byte*, uint, NativeChunkViewIntent*, NativeChunkStreamProcessWritePlan*, int>)Export(library, "octaryn_server_chunk_stream_read_process_intent");
        ProcessWriteReasonName = (delegate* unmanaged[Cdecl]<uint, int, byte*>)Export(library, "octaryn_server_chunk_stream_process_write_reason_name");
        PlanProcessStage = (delegate* unmanaged[Cdecl]<IntPtr, NativeChunkViewIntent*, uint, uint, uint, NativeChunkStreamProcessStagePlan*, int>)Export(library, "octaryn_server_chunk_stream_plan_process_stage");
        ExecuteProcessTick = (delegate* unmanaged[Cdecl]<NativeChunkStreamProcessTickDecision*, HostFrameSnapshot*, delegate* unmanaged[Cdecl]<void*, HostFrameSnapshot*, int>, delegate* unmanaged[Cdecl]<void*, HostFrameSnapshot*, int>, void*, int>)Export(library, "octaryn_server_chunk_stream_execute_process_tick");
        CreateProcessFrame = (delegate* unmanaged[Cdecl]<HostFrameSnapshot*, int>)Export(library, "octaryn_server_chunk_stream_create_process_frame");
    }

    private static nint Export(nint library, string name)
    {
        try
        {
            return NativeLibrary.GetExport(library, name);
        }
        catch (EntryPointNotFoundException)
        {
            throw new EntryPointNotFoundException($"Session stream export missing: {name}");
        }
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_SESSION_STREAM_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeSessionStreamLibrary).Assembly.Location;
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
