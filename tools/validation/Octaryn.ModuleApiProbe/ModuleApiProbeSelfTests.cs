using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.GameModules;

internal static partial class ModuleApiProbe
{
    private static List<string> RunSelfTests()
    {
        var errors = new List<string>();
        VerifyFrameworkGroupClassification(errors);
        VerifyDenied("filesystem using", "using System.IO;", FrameworkApiGroupIds.BclFilesystem, errors);
        VerifyDenied("filesystem alias", "using FileAlias = System.IO.File;", FrameworkApiGroupIds.BclFilesystem, errors);
        VerifyDenied(
            "filesystem static using",
            "using static System.IO.File;",
            FrameworkApiGroupIds.BclFilesystem,
            errors);
        VerifyDenied(
            "filesystem global type",
            "public static class Probe { public static void Run() { _ = global::System.IO.File.ReadAllText(\"x\"); } }",
            FrameworkApiGroupIds.BclFilesystem,
            errors);
        VerifyDenied(
            "environment global type",
            "public static class Probe { public static void Run() { _ = global::System.Environment.GetEnvironmentVariable(\"X\"); } }",
            FrameworkApiGroupIds.BclEnvironment,
            errors);
        VerifyDenied(
            "denied framework group constant",
            string.Empty,
            FrameworkApiGroupIds.BclFilesystem,
            errors,
            requestedGroups: ["FrameworkApiGroupIds.BclFilesystem"]);
        VerifyDenied(
            "denied framework group literal",
            string.Empty,
            FrameworkApiGroupIds.BclFilesystem,
            errors,
            requestedGroups: ["\"bcl.filesystem\""]);
        VerifyDenied(
            "networking contract namespace",
            "using Octaryn.Shared.Networking; public static class Probe { public static void Run() { _ = default(ClientCommandFrame); } }",
            "denied module API namespace",
            errors);
        ExpectValid(
            "requested allowed text API",
            "using System.Text; public static class Probe { public static void Run() { _ = new StringBuilder(); } }",
            ["FrameworkApiGroupIds.BclPrimitives", "FrameworkApiGroupIds.BclText"],
            errors);
        ExpectValid(
            "requested allowed collection API",
            "using System.Collections.Generic; public static class Probe { public static void Run() { _ = new List<int>(); } }",
            ["FrameworkApiGroupIds.BclPrimitives", "FrameworkApiGroupIds.BclCollections"],
            errors);
        ExpectValid(
            "requested non-generic collection API",
            "using System.Collections; public static class Probe { public static void Run() { _ = new ArrayList(); } }",
            ["FrameworkApiGroupIds.BclPrimitives", "FrameworkApiGroupIds.BclCollections"],
            errors);
        ExpectValid(
            "requested allowed math API",
            "public static class Probe { public static void Run() { _ = System.Math.Abs(-1); } }",
            ["FrameworkApiGroupIds.BclPrimitives", "FrameworkApiGroupIds.BclMath"],
            errors);
        ExpectValid(
            "requested allowed time API",
            "public static class Probe { public static void Run() { _ = System.DateTime.UtcNow; } }",
            ["FrameworkApiGroupIds.BclPrimitives", "FrameworkApiGroupIds.BclTime"],
            errors);
        ExpectValid(
            "manifest extractor ignores stray denied string",
            "public static class Probe { private const string Stray = \"bcl.filesystem\"; }",
            ["FrameworkApiGroupIds.BclPrimitives"],
            errors);
        ExpectValid(
            "manifest extractor ignores stray denied constant",
            "public static class Probe { private static readonly string Stray = FrameworkApiGroupIds.BclFilesystem; }",
            ["FrameworkApiGroupIds.BclPrimitives"],
            errors);
        ExpectValid(
            "module block edit command request",
            "using Octaryn.Shared.World; public static class Probe { public static void Run() { _ = ModuleCommandRequest.BreakBlock(new BlockPosition(1, 2, 3), 4); } }",
            ["FrameworkApiGroupIds.BclPrimitives"],
            errors);
        VerifyDenied(
            "unrequested allowed namespace",
            "using System.Text;",
            "unrequested framework API group bcl.text",
            errors);
        VerifyDenied(
            "unrequested allowed type",
            "public static class Probe { public static void Run() { _ = new System.Text.StringBuilder(); } }",
            "unrequested framework API group bcl.text",
            errors);
        VerifyDenied(
            "unrequested collection API",
            "using System.Collections.Generic; public static class Probe { public static void Run() { _ = new List<int>(); } }",
            "unrequested framework API group bcl.collections",
            errors);
        VerifyDenied(
            "decoy requested group does not grant API",
            "using System.Text; public static class Probe { public static void Decoy() { Register(RequestedFrameworkApiGroups: [FrameworkApiGroupIds.BclText]); } private static void Register(string[] RequestedFrameworkApiGroups) { } }",
            "unrequested framework API group bcl.text",
            errors);
        VerifyDeniedRaw(
            "fake manifest type does not grant API",
            """
            using System.Text;
            using Octaryn.Shared.FrameworkAllowlist;

            namespace FakeModule
            {
                public sealed class GameModuleManifest
                {
                    public GameModuleManifest(string[] RequestedFrameworkApiGroups) { }
                }

                public static class FakeRegistration
                {
                    public static GameModuleManifest Manifest { get; } = new(RequestedFrameworkApiGroups: [FrameworkApiGroupIds.BclText]);
                }
            }

            internal static class RealRegistration
            {
                public static Octaryn.Shared.GameModules.GameModuleManifest Manifest { get; } = new(
                    ModuleId: "octaryn.test",
                    DisplayName: "Octaryn Test",
                    Version: "0.1.0",
                    OctarynApiVersion: "0.1.0",
                    RequiredCapabilities: [],
                    RequestedHostApis: [],
                    RequestedRuntimePackages: [],
                    RequestedBuildPackages: [],
                    RequestedFrameworkApiGroups: [FrameworkApiGroupIds.BclPrimitives],
                    ModuleDependencies: [],
                    ContentDeclarations: [],
                    AssetDeclarations: [],
                    Schedule: new Octaryn.Shared.GameModules.GameModuleScheduleDeclaration([]),
                    Compatibility: new Octaryn.Shared.GameModules.GameModuleCompatibility("0.1.0", "0.1.0", "octaryn.test.save.v0", SupportsMultiplayer: false));
            }

            public static class Probe { public static void Run() { _ = new StringBuilder(); } }
            """,
            "unrequested framework API group bcl.text",
            errors);
        ExpectValidRaw(
            "fully qualified manifest construction",
            """
            using System.Text;
            using Octaryn.Shared.FrameworkAllowlist;

            internal static class RealRegistration
            {
                public static Octaryn.Shared.GameModules.GameModuleManifest Manifest { get; } =
                    new Octaryn.Shared.GameModules.GameModuleManifest(
                        ModuleId: "octaryn.test",
                        DisplayName: "Octaryn Test",
                        Version: "0.1.0",
                        OctarynApiVersion: "0.1.0",
                        RequiredCapabilities: [],
                        RequestedHostApis: [],
                        RequestedRuntimePackages: [],
                        RequestedBuildPackages: [],
                        RequestedFrameworkApiGroups: [FrameworkApiGroupIds.BclPrimitives, FrameworkApiGroupIds.BclText],
                        ModuleDependencies: [],
                        ContentDeclarations: [],
                        AssetDeclarations: [],
                        Schedule: new Octaryn.Shared.GameModules.GameModuleScheduleDeclaration([]),
                        Compatibility: new Octaryn.Shared.GameModules.GameModuleCompatibility("0.1.0", "0.1.0", "octaryn.test.save.v0", SupportsMultiplayer: false));
            }

            public static class Probe { public static void Run() { _ = new StringBuilder(); } }
            """,
            errors);
        VerifyDenied(
            "unclassified system API",
            "using System.Globalization; public static class Probe { public static void Run() { _ = CultureInfo.InvariantCulture; } }",
            "unclassified framework API",
            errors);
        VerifyDenied(
            "unrequested math API",
            "public static class Probe { public static void Run() { _ = System.Math.Abs(-1); } }",
            "unrequested framework API group bcl.math",
            errors);
        VerifyDenied(
            "unrequested time API",
            "public static class Probe { public static void Run() { _ = System.DateTime.UtcNow; } }",
            "unrequested framework API group bcl.time",
            errors);
        VerifyDenied(
            "reflection type metadata",
            "public static class Probe { public static void Run() { _ = typeof(Probe).GetMethods(); } }",
            FrameworkApiGroupIds.BclReflection,
            errors);
        VerifyDenied(
            "reflection typeof expression",
            "public static class Probe { public static void Run() { _ = typeof(Probe); } }",
            FrameworkApiGroupIds.BclReflection,
            errors);
        VerifyDenied(
            "reflection get type",
            "public static class Probe { public static void Run(object value) { _ = value.GetType(); } }",
            FrameworkApiGroupIds.BclReflection,
            errors);
        VerifyDenied(
            "reflection activator",
            "public static class Probe { public static void Run() { _ = global::System.Activator.CreateInstance(typeof(Probe)); } }",
            FrameworkApiGroupIds.BclReflection,
            errors);
        VerifyDenied(
            "reflection delegate",
            "public static class Probe { public static void Run() { _ = global::System.Delegate.CreateDelegate(typeof(System.Action), null, \"x\", false); } }",
            FrameworkApiGroupIds.BclReflection,
            errors);
        VerifyDenied(
            "reflection app domain",
            "public static class Probe { public static void Run() { _ = global::System.AppDomain.CurrentDomain.GetAssemblies(); } }",
            FrameworkApiGroupIds.BclReflection,
            errors);
        VerifyDenied(
            "runtime code generation expressions",
            "public static class Probe { public static void Run() { _ = System.Linq.Expressions.Expression.Empty(); } }",
            FrameworkApiGroupIds.BclRuntimeCodeGeneration,
            errors);
        VerifyDenied(
            "process start info",
            "public static class Probe { public static void Run() { _ = new global::System.Diagnostics.ProcessStartInfo(\"x\"); } }",
            FrameworkApiGroupIds.BclProcess,
            errors);
        VerifyDenied(
            "process start",
            "public static class Probe { public static void Run() { _ = global::System.Diagnostics.Process.Start(\"x\"); } }",
            FrameworkApiGroupIds.BclProcess,
            errors);
        VerifyDenied(
            "thread global type",
            "public static class Probe { public static void Run() { global::System.Threading.Thread.Sleep(1); } }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "task global type",
            "public static class Probe { public static void Run() { _ = new global::System.Threading.Tasks.Task(() => { }); } }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "task run",
            "public static class Probe { public static void Run() { _ = global::System.Threading.Tasks.Task.Run(() => { }); } }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "parallel for",
            "public static class Probe { public static void Run() { global::System.Threading.Tasks.Parallel.For(0, 1, _ => { }); } }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "thread pool queue",
            "public static class Probe { public static void Run() { global::System.Threading.ThreadPool.QueueUserWorkItem(_ => { }); } }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "monitor enter",
            "public static class Probe { public static void Run(object gate) { global::System.Threading.Monitor.Enter(gate); } }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "thread static",
            "public static class Probe { [System.ThreadStatic] private static int Value; }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "lock statement",
            "public static class Probe { public static void Run(object gate) { lock (gate) { } } }",
            FrameworkApiGroupIds.BclThreading,
            errors);
        VerifyDenied(
            "native import attribute",
            "using System.Runtime.InteropServices; public static partial class Probe { [DllImport(\"x\")] public static extern void Run(); }",
            FrameworkApiGroupIds.BclNativeInterop,
            errors);
        VerifyDenied(
            "fully qualified native import attribute",
            "public static partial class Probe { [System.Runtime.InteropServices.DllImport(\"x\")] public static extern void Run(); }",
            FrameworkApiGroupIds.BclNativeInterop,
            errors);
        VerifyDenied(
            "unsafe pointer",
            "public static unsafe class Probe { public static void Run() { int* value = stackalloc int[1]; } }",
            FrameworkApiGroupIds.BclUnsafeCode,
            errors);
        VerifyDenied(
            "unsafe modifier",
            "public static unsafe class Probe { public static void Run() { } }",
            FrameworkApiGroupIds.BclUnsafeCode,
            errors);
        VerifyDenied(
            "fixed statement",
            "public static unsafe class Probe { public static void Run(char[] input) { fixed (char* value = input) { } } }",
            FrameworkApiGroupIds.BclUnsafeCode,
            errors);
        VerifyDenied(
            "function pointer",
            "public static unsafe class Probe { public static void Run() { delegate*<void> value = null; } }",
            FrameworkApiGroupIds.BclUnsafeCode,
            errors);
        VerifyDenied(
            "host command write scope",
            "using Octaryn.Shared.Host; public static class Probe { public static void Run() { _ = NativeCommandWriteScope.Enter(); } }",
            "denied host control API",
            errors);
        VerifyDenied(
            "host command primitive",
            "using Octaryn.Shared.Host; public static class Probe { public static void Run() { _ = default(HostCommand); } }",
            "denied host control API",
            errors);
        VerifyDenied(
            "host frame snapshot primitive",
            "using Octaryn.Shared.Host; public static class Probe { public static void Run() { _ = default(HostFrameSnapshot); } }",
            "denied host control API",
            errors);
        VerifyDenied(
            "chunk snapshot primitive",
            "using Octaryn.Shared.World; public static class Probe { public static void Run() { _ = default(ChunkSnapshot); } }",
            "denied host control API",
            errors);
        VerifyDenied(
            "transitive scheduler namespace",
            "using Schedulers; public static class Probe { public static void Run() { } }",
            "denied module API namespace",
            errors);
        VerifyDeniedRaw(
            "unresolved type compile diagnostic",
            "public static class Probe { public static void Run() { MissingType value = null; } }",
            "compile error CS0246",
            errors);
        VerifyDeniedRaw(
            "syntax compile diagnostic",
            "public static class Probe { public static void Run() { int value = 1 } }",
            "compile error CS1002",
            errors);

        return errors;
    }

}
