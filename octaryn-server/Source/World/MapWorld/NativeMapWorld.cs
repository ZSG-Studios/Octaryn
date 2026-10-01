using System.Runtime.InteropServices;
using Octaryn.Server.Simulation.Players;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.World.MapWorld;

// P/Invoke for the native GLB map world (octaryn_server_map_world).
internal static unsafe class NativeMapWorld
{
    private const string LibraryName = "octaryn_server_map_world";

    private static readonly delegate* unmanaged[Cdecl]<byte*, byte*, IntPtr> s_create;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, void> s_destroy;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeState*, int> s_spawn;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeInput*, double, NativeState*, NativeTickResult*, int> s_step;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> s_triangleCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, float, float, float, float, float, float, float, HostRaycastHitNative*, int> s_raycast;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, HostWorldItemStateNative*, double, int> s_stepItem;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeState*, NativeInput*, double, int> s_collisionReady;

    static NativeMapWorld()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        s_create = (delegate* unmanaged[Cdecl]<byte*, byte*, IntPtr>)NativeLibrary.GetExport(
            library,
            "octaryn_server_map_world_create");
        s_destroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_map_world_destroy");
        s_spawn = (delegate* unmanaged[Cdecl]<IntPtr, NativeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_map_world_spawn");
        s_step = (delegate* unmanaged[Cdecl]<IntPtr, NativeInput*, double, NativeState*, NativeTickResult*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_map_world_step");
        s_triangleCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)NativeLibrary.GetExport(
            library,
            "octaryn_server_map_world_triangle_count");
        s_raycast = (delegate* unmanaged[Cdecl]<IntPtr, float, float, float, float, float, float, float, HostRaycastHitNative*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_map_world_raycast");
        s_stepItem = (delegate* unmanaged[Cdecl]<IntPtr, HostWorldItemStateNative*, double, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_map_world_step_item");
        s_collisionReady = (delegate* unmanaged[Cdecl]<IntPtr, NativeState*, NativeInput*, double, int>)NativeLibrary.GetExport(
            library, "octaryn_server_map_world_collision_ready_state");
    }

    public static IntPtr Create(string glbPath, string? manifestPath)
    {
        var glbPointer = Marshal.StringToCoTaskMemUTF8(glbPath);
        var manifestPointer = string.IsNullOrWhiteSpace(manifestPath)
            ? IntPtr.Zero
            : Marshal.StringToCoTaskMemUTF8(manifestPath);
        try
        {
            return s_create((byte*)glbPointer, (byte*)manifestPointer);
        }
        finally
        {
            Marshal.FreeCoTaskMem(glbPointer);
            if (manifestPointer != IntPtr.Zero)
            {
                Marshal.FreeCoTaskMem(manifestPointer);
            }
        }
    }

    public static void Destroy(IntPtr handle)
    {
        if (handle != IntPtr.Zero)
        {
            s_destroy(handle);
        }
    }

    public static NativeState SpawnState(IntPtr handle)
    {
        var nativeState = default(NativeState);
        var result = s_spawn(handle, &nativeState);
        if (result != 0)
        {
            throw new InvalidOperationException("Native map world spawn failed.");
        }

        return nativeState;
    }

    public static int SpawnInto(IntPtr handle, NativeState* state) => s_spawn(handle, state);

    public static int StepInto(IntPtr handle, NativeInput* input, double deltaSeconds, NativeState* state, NativeTickResult* result) =>
        s_step(handle, input, deltaSeconds, state, result);

    public static ulong TriangleCount(IntPtr handle) => s_triangleCount(handle);

    public static bool CollisionReady(IntPtr handle, NativeState state, NativeInput input, double deltaSeconds)
    {
        var result = s_collisionReady(handle, &state, &input, deltaSeconds);
        if (result < 0) throw new InvalidOperationException("Authority collision residency failed; see server_collision_tile diagnostic.");
        return result == 0;
    }

    public static int StepItem(IntPtr handle, ref HostWorldItemStateNative state, double deltaSeconds)
    {
        fixed (HostWorldItemStateNative* pointer = &state)
        {
            return s_stepItem(handle, pointer, deltaSeconds);
        }
    }

    public static int Raycast(
        IntPtr handle,
        float originX, float originY, float originZ,
        float directionX, float directionY, float directionZ,
        float maxDistance, out HostRaycastHit hit)
    {
        var nativeHit = default(HostRaycastHitNative);
        var result = s_raycast(
            handle,
            originX, originY, originZ,
            directionX, directionY, directionZ,
            maxDistance, &nativeHit);
        if (result != 0 || nativeHit.Hit == 0)
        {
            hit = default;
            return result;
        }

        hit = new HostRaycastHit(
            nativeHit.MaterialId,
            nativeHit.PointX, nativeHit.PointY, nativeHit.PointZ,
            nativeHit.NormalX, nativeHit.NormalY, nativeHit.NormalZ,
            nativeHit.Distance, nativeHit.TriangleIndex);
        return 0;
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_MAP_WORLD_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeMapWorld).Assembly.Location;
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
