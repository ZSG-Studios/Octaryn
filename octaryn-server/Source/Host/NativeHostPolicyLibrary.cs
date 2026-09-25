using System.Runtime.InteropServices;
using System.Runtime.ExceptionServices;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Host;

internal static unsafe class NativeHostPolicyLibrary
{
    private const string LibraryName = "octaryn_server_host";

    private static readonly delegate* unmanaged[Cdecl]<NativeHostStartupPolicy> GetStartupPolicyNative;
    private static readonly delegate* unmanaged[Cdecl]<NativeHostLiveStreamPaths> GetLiveStreamPathsNative;
    private static readonly delegate* unmanaged[Cdecl]<NativeHostLiveStreamPaths*, NativeHostLiveStreamRequestPlan> PlanLiveStreamRequestNative;
    private static readonly delegate* unmanaged[Cdecl]<uint, byte*> LiveStreamRequestReasonNameNative;
    private static readonly delegate* unmanaged[Cdecl]<HostFrameSnapshot*, void> CreateStartupFrameNative;
    private static readonly delegate* unmanaged[Cdecl]<uint, delegate* unmanaged[Cdecl]<void*, int>, void*, int> RunLiveStreamLoopNative;

    static NativeHostPolicyLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        GetStartupPolicyNative = (delegate* unmanaged[Cdecl]<NativeHostStartupPolicy>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_get_startup_policy");
        GetLiveStreamPathsNative = (delegate* unmanaged[Cdecl]<NativeHostLiveStreamPaths>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_get_live_stream_paths");
        PlanLiveStreamRequestNative = (delegate* unmanaged[Cdecl]<NativeHostLiveStreamPaths*, NativeHostLiveStreamRequestPlan>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_plan_live_stream_request");
        LiveStreamRequestReasonNameNative = (delegate* unmanaged[Cdecl]<uint, byte*>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_live_stream_request_reason_name");
        CreateStartupFrameNative = (delegate* unmanaged[Cdecl]<HostFrameSnapshot*, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_create_startup_frame");
        RunLiveStreamLoopNative = (delegate* unmanaged[Cdecl]<uint, delegate* unmanaged[Cdecl]<void*, int>, void*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_host_run_live_stream_loop");
    }

    public static NativeHostStartupPolicy GetStartupPolicy()
    {
        return GetStartupPolicyNative();
    }

    public static NativeHostLiveStreamPaths GetLiveStreamPaths()
    {
        return GetLiveStreamPathsNative();
    }

    public static NativeHostLiveStreamRequestPlan PlanLiveStreamRequest(NativeHostLiveStreamPaths paths)
    {
        return PlanLiveStreamRequestNative(&paths);
    }

    public static string LiveStreamRequestReasonName(uint reason)
    {
        return Marshal.PtrToStringUTF8((IntPtr)LiveStreamRequestReasonNameNative(reason)) ?? "none";
    }

    public static HostFrameSnapshot CreateStartupFrame()
    {
        HostFrameSnapshot frame = default;
        CreateStartupFrameNative(&frame);
        return frame;
    }

    public static int RunLiveStreamLoop(uint intervalMilliseconds, Func<int> iteration)
    {
        ArgumentNullException.ThrowIfNull(iteration);
        var state = new LiveStreamLoopIterationState(iteration);
        var handle = GCHandle.Alloc(state);
        try
        {
            var result = RunLiveStreamLoopNative(
                intervalMilliseconds,
                &RunLiveStreamIteration,
                (void*)GCHandle.ToIntPtr(handle));
            state.Exception?.Throw();
            return result;
        }
        finally
        {
            handle.Free();
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    private static int RunLiveStreamIteration(void* context)
    {
        if (context is null)
        {
            return -1;
        }

        var handle = GCHandle.FromIntPtr((IntPtr)context);
        if (handle.Target is not LiveStreamLoopIterationState state)
        {
            return -1;
        }

        try
        {
            return state.Iteration();
        }
        catch (Exception ex)
        {
            state.Exception = ExceptionDispatchInfo.Capture(ex);
            return -1;
        }
    }

    private sealed class LiveStreamLoopIterationState(Func<int> iteration)
    {
        public Func<int> Iteration { get; } = iteration;

        public ExceptionDispatchInfo? Exception { get; set; }
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
