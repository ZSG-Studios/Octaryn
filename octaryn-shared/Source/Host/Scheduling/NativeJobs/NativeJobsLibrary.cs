namespace Octaryn.Shared.Host;

using System.Runtime.InteropServices;

internal static unsafe class NativeJobsLibrary
{
    private const string LibraryName = "octaryn_native_jobs";

    public static readonly delegate* unmanaged[Cdecl]<uint> CommandWriteScopeEnter;
    public static readonly delegate* unmanaged[Cdecl]<uint> CommandWriteScopeExit;
    public static readonly delegate* unmanaged[Cdecl]<uint> CommandWriteScopeDepth;
    public static readonly delegate* unmanaged[Cdecl]<int> CommandWriteScopeIsActive;

    static NativeJobsLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        CommandWriteScopeEnter = (delegate* unmanaged[Cdecl]<uint>)Export(
            library,
            "octaryn_native_command_write_scope_enter");
        CommandWriteScopeExit = (delegate* unmanaged[Cdecl]<uint>)Export(
            library,
            "octaryn_native_command_write_scope_exit");
        CommandWriteScopeDepth = (delegate* unmanaged[Cdecl]<uint>)Export(
            library,
            "octaryn_native_command_write_scope_depth");
        CommandWriteScopeIsActive = (delegate* unmanaged[Cdecl]<int>)Export(
            library,
            "octaryn_native_command_write_scope_is_active");
    }

    public static bool IsCommandWriteScopeActive => CommandWriteScopeIsActive() != 0;

    public static uint EnterCommandWriteScope()
    {
        return CommandWriteScopeEnter();
    }

    public static uint ExitCommandWriteScope()
    {
        return CommandWriteScopeExit();
    }

    private static IntPtr Export(IntPtr library, string name)
    {
        return NativeLibrary.GetExport(library, name);
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_NATIVE_JOBS_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeJobsLibrary).Assembly.Location;
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
