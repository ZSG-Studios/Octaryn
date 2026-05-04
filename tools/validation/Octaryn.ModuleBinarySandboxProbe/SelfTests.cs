using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Octaryn.Shared.FrameworkAllowlist;

internal static class SelfTests
{
    public static List<string> Run()
    {
        var errors = new List<string>();
        ExpectDenied(errors, "filesystem", "System.IO", "File", null, FrameworkApiGroupIds.BclFilesystem);
        ExpectDenied(errors, "networking", "System.Net.Http", "HttpClient", null, FrameworkApiGroupIds.BclNetworking);
        ExpectDenied(errors, "process", "System.Diagnostics", "Process", "Start", "System.Diagnostics.Process");
        ExpectDenied(errors, "reflection type", "System", "Type", "GetMethods", FrameworkApiGroupIds.BclReflection);
        ExpectDenied(errors, "reflection assembly", "System.Reflection", "Assembly", "Load", "System.Reflection.Assembly");
        ExpectDenied(errors, "runtime loader", "System.Runtime.Loader", "AssemblyLoadContext", null, "System.Runtime.Loader.AssemblyLoadContext");
        ExpectDenied(errors, "native marshal", "System.Runtime.InteropServices", "Marshal", null, "System.Runtime.InteropServices.Marshal");
        ExpectDenied(errors, "threading task", "System.Threading.Tasks", "Task", "Run", FrameworkApiGroupIds.BclThreading);
        ExpectDenied(errors, "threading parallel", "System.Threading.Tasks", "Parallel", "For", FrameworkApiGroupIds.BclThreading);
        ExpectDenied(errors, "networking contract namespace", "Octaryn.Shared.Networking", "ClientCommandFrame", null, "denied module API namespace");
        ExpectDenied(errors, "transitive scheduler namespace", "Schedulers", "JobScheduler", null, "denied module API namespace");
        ExpectAllowed(errors, "compiler attribute", "System.Runtime.CompilerServices", "NullableContextAttribute", ".ctor");
        ValidateAllowedAssemblyReferencePolicy(errors);
        return errors;
    }

    private static void ValidateAllowedAssemblyReferencePolicy(List<string> errors)
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), $"octaryn-binary-sandbox-{Guid.NewGuid():N}");
        try
        {
            Directory.CreateDirectory(tempRoot);
            var assetsPath = Path.Combine(tempRoot, "project.assets.json");
            var policyPath = Path.Combine(tempRoot, "module-packages.json");
            File.WriteAllText(policyPath, """
                {
                  "runtimeDirect": {"Arch": "2.1.0"},
                  "runtimeTransitive": {"Good.Transitive": "1.0.0"}
                }
                """);
            var validAssets = """
                {
                  "projectFileDependencyGroups": {
                    "net10.0": [
                      "Arch >= 2.1.0",
                      "Octaryn.Shared >= 1.0.0"
                    ]
                  },
                  "targets": {
                    "net10.0": {
                      "Arch/2.1.0": {
                        "dependencies": {
                          "Good.Transitive": "1.0.0",
                          "Bad.Transitive": "1.0.0"
                        },
                        "compile": {"lib/net8.0/Arch.dll": {}},
                        "runtime": {"lib/net8.0/Arch.dll": {}}
                      },
                      "Good.Transitive/1.0.0": {
                        "compile": {"lib/net8.0/Good.Transitive.dll": {}},
                        "runtime": {"lib/net8.0/Good.Transitive.dll": {}}
                      },
                      "Bad.Transitive/1.0.0": {
                        "compile": {"lib/net8.0/Bad.Transitive.dll": {}},
                        "runtime": {"lib/net8.0/Bad.Transitive.dll": {}}
                      },
                      "Octaryn.Shared/1.0.0": {
                        "type": "project",
                        "compile": {"bin/placeholder/Octaryn.Shared.dll": {}},
                        "runtime": {"bin/placeholder/Octaryn.Shared.dll": {}}
                      }
                    }
                  },
                  "libraries": {
                    "Arch/2.1.0": {"type": "package", "path": "arch/2.1.0"},
                    "Good.Transitive/1.0.0": {"type": "package", "path": "good.transitive/1.0.0"},
                    "Bad.Transitive/1.0.0": {"type": "package", "path": "bad.transitive/1.0.0"},
                    "Octaryn.Shared/1.0.0": {"type": "project", "path": "../octaryn-shared/Octaryn.Shared.csproj"}
                  }
                }
                """;
            File.WriteAllText(assetsPath, validAssets);

            var allowed = AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assetsPath, policyPath, errors);
            ExpectAllowedAssembly(errors, allowed, "Arch");
            ExpectAllowedAssembly(errors, allowed, "Good.Transitive");
            ExpectAllowedAssembly(errors, allowed, "Octaryn.Shared");
            ExpectDeniedAssembly(errors, allowed, "Bad.Transitive");

            File.WriteAllText(assetsPath, File.ReadAllText(assetsPath).Replace(
                "../octaryn-shared/Octaryn.Shared.csproj",
                "../spoof/Octaryn.Shared.csproj"));
            var spoofedSharedAllowed = AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assetsPath, policyPath, errors);
            ExpectDeniedAssembly(errors, spoofedSharedAllowed, "Octaryn.Shared");

            File.WriteAllText(assetsPath, validAssets.Replace(
                "\"Octaryn.Shared/1.0.0\": {\"type\": \"project\", \"path\": \"../octaryn-shared/Octaryn.Shared.csproj\"}",
                "\"Octaryn.Shared/1.0.0\": {\"type\": \"package\", \"path\": \"octaryn.shared/1.0.0\"}"));
            var packageSharedAllowed = AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assetsPath, policyPath, errors);
            ExpectDeniedAssembly(errors, packageSharedAllowed, "Octaryn.Shared");

            File.WriteAllText(assetsPath, validAssets.Replace(
                "\"Octaryn.Shared/1.0.0\": {\"type\": \"project\", \"path\": \"../octaryn-shared/Octaryn.Shared.csproj\"}",
                "\"Octaryn.Shared/1.0.0\": {\"type\": \"project\"}"));
            var missingPathSharedAllowed = AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assetsPath, policyPath, errors);
            ExpectDeniedAssembly(errors, missingPathSharedAllowed, "Octaryn.Shared");
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }

    private static void ExpectAllowedAssembly(List<string> errors, IReadOnlySet<string> allowed, string assemblyName)
    {
        if (!allowed.Contains(assemblyName))
        {
            errors.Add($"assembly policy: expected allowed assembly {assemblyName}");
        }
    }

    private static void ExpectDeniedAssembly(List<string> errors, IReadOnlySet<string> allowed, string assemblyName)
    {
        if (allowed.Contains(assemblyName))
        {
            errors.Add($"assembly policy: expected denied assembly {assemblyName}");
        }
    }

    private static void ExpectDenied(
        List<string> errors,
        string name,
        string namespaceName,
        string typeName,
        string? memberName,
        string expectedText)
    {
        var classification = new List<string>();
        ApiClassifier.ValidateTypeName(classification, "self-test.dll", namespaceName, typeName, memberName);
        if (!classification.Any(error => error.Contains(expectedText, StringComparison.Ordinal)))
        {
            errors.Add($"{name}: expected denied metadata containing {expectedText}, got {string.Join(", ", classification)}");
        }
    }

    private static void ExpectAllowed(
        List<string> errors,
        string name,
        string namespaceName,
        string typeName,
        string? memberName)
    {
        var classification = new List<string>();
        ApiClassifier.ValidateTypeName(classification, "self-test.dll", namespaceName, typeName, memberName);
        if (classification.Count > 0)
        {
            errors.Add($"{name}: expected allowed metadata, got {string.Join(", ", classification)}");
        }
    }
}
