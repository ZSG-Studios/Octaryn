namespace Octaryn.Shared.ModuleSandbox;

using Octaryn.Shared.FrameworkAllowlist;

public static class DeniedFrameworkApiGroups
{
    public static readonly IReadOnlyList<string> Values =
    [
        FrameworkApiGroupIds.BclFilesystem,
        FrameworkApiGroupIds.BclNetworking,
        FrameworkApiGroupIds.BclProcess,
        FrameworkApiGroupIds.BclReflection,
        FrameworkApiGroupIds.BclRuntimeCodeGeneration,
        FrameworkApiGroupIds.BclNativeInterop,
        FrameworkApiGroupIds.BclThreading,
        FrameworkApiGroupIds.BclEnvironment,
        FrameworkApiGroupIds.BclConsole,
        FrameworkApiGroupIds.BclUnsafeCode
    ];
}
