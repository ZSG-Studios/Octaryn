using System.Collections.Generic;
using Octaryn.Shared.FrameworkAllowlist;

internal static class ApiClassifier
{
    private static readonly IReadOnlyDictionary<string, string> DeniedNamespaces = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["System.Console"] = FrameworkApiGroupIds.BclConsole,
        ["System.Environment"] = FrameworkApiGroupIds.BclEnvironment,
        ["System.IO"] = FrameworkApiGroupIds.BclFilesystem,
        ["System.Linq.Expressions"] = FrameworkApiGroupIds.BclRuntimeCodeGeneration,
        ["System.Net"] = FrameworkApiGroupIds.BclNetworking,
        ["System.Reflection.Emit"] = FrameworkApiGroupIds.BclRuntimeCodeGeneration,
        ["System.Runtime.InteropServices"] = FrameworkApiGroupIds.BclNativeInterop,
        ["System.Runtime.Loader"] = FrameworkApiGroupIds.BclRuntimeCodeGeneration,
        ["System.Threading"] = FrameworkApiGroupIds.BclThreading,
        ["System.Threading.Tasks"] = FrameworkApiGroupIds.BclThreading,
        ["System.Timers"] = FrameworkApiGroupIds.BclThreading
    };

    private static readonly IReadOnlyDictionary<string, string> DeniedTypeNames = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["Activator"] = FrameworkApiGroupIds.BclReflection,
        ["AppDomain"] = FrameworkApiGroupIds.BclReflection,
        ["Console"] = FrameworkApiGroupIds.BclConsole,
        ["Delegate"] = FrameworkApiGroupIds.BclReflection,
        ["Directory"] = FrameworkApiGroupIds.BclFilesystem,
        ["DirectoryInfo"] = FrameworkApiGroupIds.BclFilesystem,
        ["Environment"] = FrameworkApiGroupIds.BclEnvironment,
        ["File"] = FrameworkApiGroupIds.BclFilesystem,
        ["FileInfo"] = FrameworkApiGroupIds.BclFilesystem,
        ["FileStream"] = FrameworkApiGroupIds.BclFilesystem,
        ["HttpClient"] = FrameworkApiGroupIds.BclNetworking,
        ["Monitor"] = FrameworkApiGroupIds.BclThreading,
        ["Parallel"] = FrameworkApiGroupIds.BclThreading,
        ["Path"] = FrameworkApiGroupIds.BclFilesystem,
        ["Process"] = FrameworkApiGroupIds.BclProcess,
        ["ProcessStartInfo"] = FrameworkApiGroupIds.BclProcess,
        ["SemaphoreSlim"] = FrameworkApiGroupIds.BclThreading,
        ["Socket"] = FrameworkApiGroupIds.BclNetworking,
        ["StreamReader"] = FrameworkApiGroupIds.BclFilesystem,
        ["StreamWriter"] = FrameworkApiGroupIds.BclFilesystem,
        ["Task"] = FrameworkApiGroupIds.BclThreading,
        ["Thread"] = FrameworkApiGroupIds.BclThreading,
        ["ThreadPool"] = FrameworkApiGroupIds.BclThreading,
        ["Timer"] = FrameworkApiGroupIds.BclThreading,
        ["UnmanagedCallersOnlyAttribute"] = FrameworkApiGroupIds.BclNativeInterop
    };

    private static readonly IReadOnlySet<string> DeniedModuleApiNamespaces = new HashSet<string>(StringComparer.Ordinal)
    {
        "Octaryn.Shared.Networking",
        "Schedulers"
    };

    private static readonly IReadOnlySet<string> DeniedFullTypeNames = new HashSet<string>(StringComparer.Ordinal)
    {
        "System.Reflection.Assembly",
        "System.Reflection.ConstructorInfo",
        "System.Reflection.EventInfo",
        "System.Reflection.FieldInfo",
        "System.Reflection.MemberInfo",
        "System.Reflection.MethodBase",
        "System.Reflection.MethodInfo",
        "System.Reflection.PropertyInfo",
        "System.Reflection.TypeInfo",
        "System.Runtime.InteropServices.DllImportAttribute",
        "System.Runtime.InteropServices.LibraryImportAttribute",
        "System.Runtime.InteropServices.Marshal",
        "System.Runtime.Loader.AssemblyLoadContext",
        "System.Diagnostics.Process",
        "System.Diagnostics.ProcessStartInfo"
    };

    public static void ValidateTypeName(
        List<string> errors,
        string assemblyPath,
        string namespaceName,
        string typeName,
        string? memberName)
    {
        if (memberName == ".ctor" && typeName.EndsWith("Attribute", StringComparison.Ordinal))
        {
            return;
        }

        var fullTypeName = string.IsNullOrEmpty(namespaceName)
            ? typeName
            : $"{namespaceName}.{typeName}";
        if (DeniedFullTypeNames.Contains(fullTypeName))
        {
            errors.Add($"{assemblyPath}: denied framework API: {fullTypeName}");
            return;
        }

        foreach (var deniedNamespace in DeniedModuleApiNamespaces)
        {
            if (namespaceName == deniedNamespace ||
                namespaceName.StartsWith(deniedNamespace + ".", StringComparison.Ordinal))
            {
                errors.Add($"{assemblyPath}: denied module API namespace: {fullTypeName}");
                return;
            }
        }

        if (fullTypeName == "System.Type" && memberName == "GetTypeFromHandle")
        {
            return;
        }

        if (fullTypeName == "System.Type" && memberName is not null)
        {
            errors.Add($"{assemblyPath}: denied framework API group {FrameworkApiGroupIds.BclReflection}: {fullTypeName}.{memberName}");
            return;
        }

        if (namespaceName == "System.Diagnostics" &&
            !typeName.StartsWith("Debugger", StringComparison.Ordinal) &&
            typeName != "DebuggableAttribute")
        {
            errors.Add($"{assemblyPath}: denied framework API group {FrameworkApiGroupIds.BclProcess}: {fullTypeName}");
            return;
        }

        if (namespaceName == "System.Reflection" && memberName is not null)
        {
            errors.Add($"{assemblyPath}: denied framework API group {FrameworkApiGroupIds.BclReflection}: {fullTypeName}");
            return;
        }

        if (namespaceName == "System.Runtime.InteropServices" &&
            typeName is not ("InAttribute" or "OutAttribute" or "OptionalAttribute"))
        {
            errors.Add($"{assemblyPath}: denied framework API group {FrameworkApiGroupIds.BclNativeInterop}: {fullTypeName}");
            return;
        }

        if (namespaceName == "System.Runtime.InteropServices")
        {
            return;
        }

        if (DeniedTypeNames.TryGetValue(typeName, out var typeGroup))
        {
            errors.Add($"{assemblyPath}: denied framework API group {typeGroup}: {fullTypeName}");
            return;
        }

        foreach (var (deniedNamespace, group) in DeniedNamespaces)
        {
            if (namespaceName == deniedNamespace ||
                namespaceName.StartsWith(deniedNamespace + ".", StringComparison.Ordinal))
            {
                errors.Add($"{assemblyPath}: denied framework API group {group}: {fullTypeName}");
                return;
            }
        }
    }
}
