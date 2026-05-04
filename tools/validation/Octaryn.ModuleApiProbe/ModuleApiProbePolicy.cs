using Octaryn.Shared.FrameworkAllowlist;

internal static partial class ModuleApiProbe
{
    private static readonly IReadOnlyDictionary<string, string> DeniedNamespaces = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["System.Console"] = FrameworkApiGroupIds.BclConsole,
        ["System.Activator"] = FrameworkApiGroupIds.BclReflection,
        ["System.AppDomain"] = FrameworkApiGroupIds.BclReflection,
        ["System.Attribute"] = FrameworkApiGroupIds.BclReflection,
        ["System.Delegate"] = FrameworkApiGroupIds.BclReflection,
        ["System.Diagnostics"] = FrameworkApiGroupIds.BclProcess,
        ["System.Environment"] = FrameworkApiGroupIds.BclEnvironment,
        ["System.IO"] = FrameworkApiGroupIds.BclFilesystem,
        ["System.Linq.Expressions"] = FrameworkApiGroupIds.BclRuntimeCodeGeneration,
        ["System.Net"] = FrameworkApiGroupIds.BclNetworking,
        ["System.Reflection"] = FrameworkApiGroupIds.BclReflection,
        ["System.Reflection.Emit"] = FrameworkApiGroupIds.BclRuntimeCodeGeneration,
        ["System.Runtime.InteropServices"] = FrameworkApiGroupIds.BclNativeInterop,
        ["System.Runtime.Loader"] = FrameworkApiGroupIds.BclRuntimeCodeGeneration,
        ["System.ThreadStaticAttribute"] = FrameworkApiGroupIds.BclThreading,
        ["System.Type"] = FrameworkApiGroupIds.BclReflection,
        ["System.Threading"] = FrameworkApiGroupIds.BclThreading,
        ["System.Threading.Tasks"] = FrameworkApiGroupIds.BclThreading,
        ["System.Timers"] = FrameworkApiGroupIds.BclThreading
    };

    private static readonly IReadOnlyDictionary<string, string> AllowedNamespaces = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["System.Collections"] = FrameworkApiGroupIds.BclCollections,
        ["System.Collections.Generic"] = FrameworkApiGroupIds.BclCollections,
        ["System.Buffers"] = FrameworkApiGroupIds.BclMemory,
        ["System.Numerics"] = FrameworkApiGroupIds.BclMath,
        ["System.Text"] = FrameworkApiGroupIds.BclText
    };

    private static readonly IReadOnlyDictionary<string, string> DeniedTypeNames = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["Console"] = FrameworkApiGroupIds.BclConsole,
        ["Activator"] = FrameworkApiGroupIds.BclReflection,
        ["AppDomain"] = FrameworkApiGroupIds.BclReflection,
        ["Attribute"] = FrameworkApiGroupIds.BclReflection,
        ["Delegate"] = FrameworkApiGroupIds.BclReflection,
        ["Directory"] = FrameworkApiGroupIds.BclFilesystem,
        ["DirectoryInfo"] = FrameworkApiGroupIds.BclFilesystem,
        ["Environment"] = FrameworkApiGroupIds.BclEnvironment,
        ["File"] = FrameworkApiGroupIds.BclFilesystem,
        ["FileInfo"] = FrameworkApiGroupIds.BclFilesystem,
        ["FileStream"] = FrameworkApiGroupIds.BclFilesystem,
        ["HttpClient"] = FrameworkApiGroupIds.BclNetworking,
        ["Path"] = FrameworkApiGroupIds.BclFilesystem,
        ["Process"] = FrameworkApiGroupIds.BclProcess,
        ["ProcessStartInfo"] = FrameworkApiGroupIds.BclProcess,
        ["Socket"] = FrameworkApiGroupIds.BclNetworking,
        ["StreamReader"] = FrameworkApiGroupIds.BclFilesystem,
        ["StreamWriter"] = FrameworkApiGroupIds.BclFilesystem,
        ["CancellationTokenSource"] = FrameworkApiGroupIds.BclThreading,
        ["Monitor"] = FrameworkApiGroupIds.BclThreading,
        ["SemaphoreSlim"] = FrameworkApiGroupIds.BclThreading,
        ["Task"] = FrameworkApiGroupIds.BclThreading,
        ["Thread"] = FrameworkApiGroupIds.BclThreading,
        ["ThreadPool"] = FrameworkApiGroupIds.BclThreading,
        ["Timer"] = FrameworkApiGroupIds.BclThreading,
        ["Type"] = FrameworkApiGroupIds.BclReflection
    };

    private static readonly IReadOnlyDictionary<string, string> AllowedTypeNames = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["Array"] = FrameworkApiGroupIds.BclPrimitives,
        ["Boolean"] = FrameworkApiGroupIds.BclPrimitives,
        ["Char"] = FrameworkApiGroupIds.BclPrimitives,
        ["DateOnly"] = FrameworkApiGroupIds.BclTime,
        ["DateTime"] = FrameworkApiGroupIds.BclTime,
        ["Decimal"] = FrameworkApiGroupIds.BclPrimitives,
        ["Double"] = FrameworkApiGroupIds.BclPrimitives,
        ["Int16"] = FrameworkApiGroupIds.BclPrimitives,
        ["Int32"] = FrameworkApiGroupIds.BclPrimitives,
        ["Int64"] = FrameworkApiGroupIds.BclPrimitives,
        ["Math"] = FrameworkApiGroupIds.BclMath,
        ["Memory"] = FrameworkApiGroupIds.BclMemory,
        ["ReadOnlyMemory"] = FrameworkApiGroupIds.BclMemory,
        ["ReadOnlySpan"] = FrameworkApiGroupIds.BclMemory,
        ["Single"] = FrameworkApiGroupIds.BclPrimitives,
        ["Span"] = FrameworkApiGroupIds.BclMemory,
        ["String"] = FrameworkApiGroupIds.BclPrimitives,
        ["StringBuilder"] = FrameworkApiGroupIds.BclText,
        ["TimeOnly"] = FrameworkApiGroupIds.BclTime,
        ["TimeSpan"] = FrameworkApiGroupIds.BclTime
    };

    private static readonly HashSet<string> DeniedHostControlTypes = new(StringComparer.Ordinal)
    {
        "Octaryn.Shared.Host.HostCommand",
        "Octaryn.Shared.Host.HostCommandWriteScope",
        "Octaryn.Shared.Host.HostFrameContext",
        "Octaryn.Shared.Host.HostFrameSnapshot",
        "Octaryn.Shared.Host.HostInputSnapshot",
        "Octaryn.Shared.Host.HostModuleContext",
        "Octaryn.Shared.Host.HostSchedulerDiagnostics",
        "Octaryn.Shared.Host.HostScheduledWork",
        "Octaryn.Shared.Host.HostScheduledWorkContext",
        "Octaryn.Shared.Host.HostSchedulingContract",
        "Octaryn.Shared.Host.HostWorkAccess",
        "Octaryn.Shared.Host.IHostCommandSink",
        "Octaryn.Shared.Host.IHostScheduler",
        "Octaryn.Shared.World.ChunkConstants",
        "Octaryn.Shared.World.ChunkPosition",
        "Octaryn.Shared.World.ChunkSnapshot",
        "Schedulers.JobScheduler",
        "Schedulers.JobHandle"
    };

    private static readonly HashSet<string> DeniedModuleApiNamespaces = new(StringComparer.Ordinal)
    {
        "Octaryn.Shared.Networking",
        "Schedulers"
    };

    private static string? ResolveFrameworkGroup(string constantName)
    {
        return constantName switch
        {
            nameof(FrameworkApiGroupIds.BclPrimitives) => FrameworkApiGroupIds.BclPrimitives,
            nameof(FrameworkApiGroupIds.BclCollections) => FrameworkApiGroupIds.BclCollections,
            nameof(FrameworkApiGroupIds.BclMemory) => FrameworkApiGroupIds.BclMemory,
            nameof(FrameworkApiGroupIds.BclMath) => FrameworkApiGroupIds.BclMath,
            nameof(FrameworkApiGroupIds.BclTime) => FrameworkApiGroupIds.BclTime,
            nameof(FrameworkApiGroupIds.BclText) => FrameworkApiGroupIds.BclText,
            nameof(FrameworkApiGroupIds.BclFilesystem) => FrameworkApiGroupIds.BclFilesystem,
            nameof(FrameworkApiGroupIds.BclNetworking) => FrameworkApiGroupIds.BclNetworking,
            nameof(FrameworkApiGroupIds.BclProcess) => FrameworkApiGroupIds.BclProcess,
            nameof(FrameworkApiGroupIds.BclReflection) => FrameworkApiGroupIds.BclReflection,
            nameof(FrameworkApiGroupIds.BclRuntimeCodeGeneration) => FrameworkApiGroupIds.BclRuntimeCodeGeneration,
            nameof(FrameworkApiGroupIds.BclNativeInterop) => FrameworkApiGroupIds.BclNativeInterop,
            nameof(FrameworkApiGroupIds.BclThreading) => FrameworkApiGroupIds.BclThreading,
            nameof(FrameworkApiGroupIds.BclEnvironment) => FrameworkApiGroupIds.BclEnvironment,
            nameof(FrameworkApiGroupIds.BclConsole) => FrameworkApiGroupIds.BclConsole,
            nameof(FrameworkApiGroupIds.BclUnsafeCode) => FrameworkApiGroupIds.BclUnsafeCode,
            _ => null
        };
    }

    private static string? FindAllowedGroup(string fullyQualifiedName)
    {
        foreach (var (prefix, group) in AllowedNamespaces)
        {
            if (fullyQualifiedName == prefix ||
                fullyQualifiedName.StartsWith($"{prefix}.", StringComparison.Ordinal))
            {
                return group;
            }
        }

        return null;
    }

    private static string? FindAllowedTypeGroup(string fullyQualifiedName)
    {
        var typeName = fullyQualifiedName.Split('.').Last();
        return AllowedTypeNames.TryGetValue(typeName, out var group) ? group : null;
    }

    private static bool IsSystemApi(string fullyQualifiedName)
    {
        return fullyQualifiedName == "System" ||
            fullyQualifiedName.StartsWith("System.", StringComparison.Ordinal);
    }

    private static string? FindDeniedGroup(string fullyQualifiedName)
    {
        foreach (var (prefix, group) in DeniedNamespaces)
        {
            if (fullyQualifiedName == prefix ||
                fullyQualifiedName.StartsWith($"{prefix}.", StringComparison.Ordinal))
            {
                return group;
            }
        }

        return null;
    }

    private static bool IsDeniedModuleApiNamespace(string fullyQualifiedName)
    {
        return DeniedModuleApiNamespaces.Any(prefix =>
            fullyQualifiedName == prefix ||
            fullyQualifiedName.StartsWith($"{prefix}.", StringComparison.Ordinal));
    }
}
