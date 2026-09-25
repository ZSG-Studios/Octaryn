using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.ModuleSandbox;

internal static partial class ModuleApiProbe
{
    private static void VerifyFrameworkGroupClassification(List<string> errors)
    {
        var allowed = new HashSet<string>(FrameworkApiGroupAllowlist.Values, StringComparer.Ordinal);
        var denied = new HashSet<string>(DeniedFrameworkApiGroups.Values, StringComparer.Ordinal);
        foreach (var field in typeof(FrameworkApiGroupIds).GetFields())
        {
            if (field.GetValue(null) is not string value)
            {
                continue;
            }

            var isAllowed = allowed.Contains(value);
            var isDenied = denied.Contains(value);
            if (isAllowed == isDenied)
            {
                errors.Add($"framework group {value} must be classified as exactly one of allowed or denied");
            }
        }
    }

    private static void VerifyDenied(
        string name,
        string source,
        string expectedText,
        List<string> errors,
        IReadOnlyList<string>? requestedGroups = null)
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), $"octaryn-module-api-probe-{Guid.NewGuid():N}");
        try
        {
            var sourceRoot = Path.Combine(tempRoot, "Source", "Module");
            Directory.CreateDirectory(sourceRoot);
            File.WriteAllText(Path.Combine(sourceRoot, "Probe.cs"), SelfTestSource(source, requestedGroups));

            var validationErrors = Validate(tempRoot);
            if (!validationErrors.Any(error => error.Contains(expectedText, StringComparison.Ordinal)))
            {
                errors.Add($"{name} did not report {expectedText}");
            }
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }

    private static void ExpectValid(
        string name,
        string source,
        IReadOnlyList<string> requestedGroups,
        List<string> errors)
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), $"octaryn-module-api-probe-{Guid.NewGuid():N}");
        try
        {
            var sourceRoot = Path.Combine(tempRoot, "Source", "Module");
            Directory.CreateDirectory(sourceRoot);
            File.WriteAllText(Path.Combine(sourceRoot, "Probe.cs"), SelfTestSource(source, requestedGroups));

            var validationErrors = Validate(tempRoot);
            if (validationErrors.Count > 0)
            {
                errors.Add($"{name} expected valid source, got {string.Join(", ", validationErrors)}");
            }
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }

    private static void VerifyDeniedRaw(
        string name,
        string source,
        string expectedText,
        List<string> errors)
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), $"octaryn-module-api-probe-{Guid.NewGuid():N}");
        try
        {
            var sourceRoot = Path.Combine(tempRoot, "Source", "Module");
            Directory.CreateDirectory(sourceRoot);
            File.WriteAllText(Path.Combine(sourceRoot, "Probe.cs"), source);

            var validationErrors = Validate(tempRoot);
            if (!validationErrors.Any(error => error.Contains(expectedText, StringComparison.Ordinal)))
            {
                errors.Add($"{name} did not report {expectedText}");
            }
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }

    private static void ExpectValidRaw(string name, string source, List<string> errors)
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), $"octaryn-module-api-probe-{Guid.NewGuid():N}");
        try
        {
            var sourceRoot = Path.Combine(tempRoot, "Source", "Module");
            Directory.CreateDirectory(sourceRoot);
            File.WriteAllText(Path.Combine(sourceRoot, "Probe.cs"), source);

            var validationErrors = Validate(tempRoot);
            if (validationErrors.Count > 0)
            {
                errors.Add($"{name} expected valid source, got {string.Join(", ", validationErrors)}");
            }
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }

    private static string SelfTestSource(string source, IReadOnlyList<string>? requestedGroups = null)
    {
        var groups = string.Join(
            "," + Environment.NewLine + "                    ",
            requestedGroups ?? ["FrameworkApiGroupIds.BclPrimitives"]);

        return $$"""
            using Octaryn.Shared.FrameworkAllowlist;
            using Octaryn.Shared.GameModules;

            {{source}}

            internal static class ModuleFrameworkGroups
            {
                public static GameModuleManifest Manifest { get; } = new(
                    ModuleId: "octaryn.test",
                    DisplayName: "Octaryn Test",
                    Version: "0.1.0",
                    OctarynApiVersion: "0.1.0",
                    RequiredCapabilities: [],
                    RequestedHostApis: [],
                    RequestedRuntimePackages: [],
                    RequestedBuildPackages: [],
                    RequestedFrameworkApiGroups:
                    [
                        {{groups}}
                    ],
                    ModuleDependencies: [],
                    ContentDeclarations: [],
                    AssetDeclarations: [],
                    Schedule: new GameModuleScheduleDeclaration([]),
                    Compatibility: new GameModuleCompatibility("0.1.0", "0.1.0", "octaryn.test.save.v0", SupportsMultiplayer: false));
            }
            """;
    }
}
