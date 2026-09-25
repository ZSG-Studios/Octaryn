using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

internal static class AssemblyReferencePolicy
{
    public static IReadOnlySet<string> LoadAllowedAssemblyReferences(
        string assetsPath,
        string? policyPath,
        List<string> errors)
    {
        var allowed = new HashSet<string>(StringComparer.Ordinal);

        if (!File.Exists(assetsPath))
        {
            errors.Add($"{assetsPath}: project assets file does not exist");
            return allowed;
        }

        var policy = LoadRuntimePackagePolicy(assetsPath, policyPath, errors);
        if (policy.Direct.Count == 0)
        {
            return allowed;
        }

        using var assetsStream = File.OpenRead(assetsPath);
        using var assetsDocument = JsonDocument.Parse(assetsStream);
        var directProjectPackages = LoadDirectProjectPackages(assetsDocument.RootElement);
        if (!assetsDocument.RootElement.TryGetProperty("targets", out var targets) ||
            targets.ValueKind != JsonValueKind.Object)
        {
            errors.Add($"{assetsPath}: project assets file has no targets object");
            return allowed;
        }

        var packageLibraries = new Dictionary<string, JsonElement>(StringComparer.Ordinal);
        var packageDependencies = new Dictionary<string, IReadOnlyList<string>>(StringComparer.Ordinal);
        foreach (var target in targets.EnumerateObject())
        {
            foreach (var library in target.Value.EnumerateObject())
            {
                var packageId = PackageId(library.Name);
                packageLibraries[packageId] = library.Value;
                packageDependencies[packageId] = DependencyIds(library.Value);
                if (packageId == "Octaryn.Shared" && IsVerifiedSharedProject(assetsDocument.RootElement, library.Name))
                {
                    AddAssetAssemblies(allowed, library.Value, "compile");
                    AddAssetAssemblies(allowed, library.Value, "runtime");
                }
            }
        }

        var runtimeClosure = RuntimeClosureFrom(directProjectPackages.Intersect(policy.Direct, StringComparer.Ordinal), packageDependencies, policy);
        foreach (var packageId in runtimeClosure)
        {
            if (packageLibraries.TryGetValue(packageId, out var library))
            {
                AddAssetAssemblies(allowed, library, "compile");
                AddAssetAssemblies(allowed, library, "runtime");
            }
        }

        return allowed;
    }

    private static IReadOnlyList<string> DependencyIds(JsonElement library)
    {
        if (!library.TryGetProperty("dependencies", out var dependencies) ||
            dependencies.ValueKind != JsonValueKind.Object)
        {
            return [];
        }

        return dependencies
            .EnumerateObject()
            .Select(dependency => dependency.Name)
            .ToArray();
    }

    private static IReadOnlySet<string> RuntimeClosureFrom(
        IEnumerable<string> roots,
        IReadOnlyDictionary<string, IReadOnlyList<string>> dependencies,
        RuntimePackagePolicy policy)
    {
        var allowedPackages = policy.Direct
            .Concat(policy.Transitive)
            .ToHashSet(StringComparer.Ordinal);
        var closure = new HashSet<string>(StringComparer.Ordinal);
        var pending = new Queue<string>(roots);
        while (pending.Count > 0)
        {
            var packageId = pending.Dequeue();
            if (!allowedPackages.Contains(packageId) || !closure.Add(packageId))
            {
                continue;
            }

            if (!dependencies.TryGetValue(packageId, out var packageDependencies))
            {
                continue;
            }

            foreach (var dependency in packageDependencies)
            {
                pending.Enqueue(dependency);
            }
        }

        return closure;
    }

    private static bool IsVerifiedSharedProject(JsonElement root, string targetKey)
    {
        if (PackageId(targetKey) != "Octaryn.Shared" ||
            !root.TryGetProperty("libraries", out var libraries) ||
            libraries.ValueKind != JsonValueKind.Object ||
            !libraries.TryGetProperty(targetKey, out var library) ||
            library.ValueKind != JsonValueKind.Object)
        {
            return false;
        }

        if (!library.TryGetProperty("type", out var type) ||
            type.GetString() != "project" ||
            !library.TryGetProperty("path", out var path))
        {
            return false;
        }

        var normalizedPath = path.GetString()?.Replace('\\', '/');
        return normalizedPath == "../octaryn-shared/Octaryn.Shared.csproj";
    }

    private static RuntimePackagePolicy LoadRuntimePackagePolicy(
        string assetsPath,
        string? policyPath,
        List<string> errors)
    {
        var resolvedPolicyPath = policyPath ?? FindDefaultPolicyPath(assetsPath);
        if (resolvedPolicyPath is null || !File.Exists(resolvedPolicyPath))
        {
            errors.Add("module package policy file does not exist");
            return new RuntimePackagePolicy(
                new HashSet<string>(StringComparer.Ordinal),
                new HashSet<string>(StringComparer.Ordinal));
        }

        using var stream = File.OpenRead(resolvedPolicyPath);
        using var document = JsonDocument.Parse(stream);
        return new RuntimePackagePolicy(
            LoadPolicyPackageSection(document.RootElement, resolvedPolicyPath, "runtimeDirect", errors),
            LoadPolicyPackageSection(document.RootElement, resolvedPolicyPath, "runtimeTransitive", errors));
    }

    private static IReadOnlySet<string> LoadPolicyPackageSection(
        JsonElement root,
        string policyPath,
        string section,
        List<string> errors)
    {
        if (!root.TryGetProperty(section, out var packages) ||
            packages.ValueKind != JsonValueKind.Object)
        {
            errors.Add($"{policyPath}: package policy has no {section} object");
            return new HashSet<string>(StringComparer.Ordinal);
        }

        return packages
            .EnumerateObject()
            .Select(package => package.Name)
            .ToHashSet(StringComparer.Ordinal);
    }

    private readonly record struct RuntimePackagePolicy(
        IReadOnlySet<string> Direct,
        IReadOnlySet<string> Transitive);

    private static string? FindDefaultPolicyPath(string assetsPath)
    {
        var directory = Path.GetDirectoryName(Path.GetFullPath(assetsPath));
        while (directory is not null)
        {
            var candidate = Path.Combine(directory, "tools", "package-policy", "module-packages.json");
            if (File.Exists(candidate))
            {
                return candidate;
            }

            directory = Directory.GetParent(directory)?.FullName;
        }

        return null;
    }

    private static IReadOnlySet<string> LoadDirectProjectPackages(JsonElement root)
    {
        var packages = new HashSet<string>(StringComparer.Ordinal);
        if (!root.TryGetProperty("projectFileDependencyGroups", out var groups) ||
            groups.ValueKind != JsonValueKind.Object)
        {
            return packages;
        }

        foreach (var group in groups.EnumerateObject())
        {
            if (group.Value.ValueKind != JsonValueKind.Array)
            {
                continue;
            }

            foreach (var dependency in group.Value.EnumerateArray())
            {
                var text = dependency.GetString();
                if (!string.IsNullOrWhiteSpace(text))
                {
                    packages.Add(text.Split(' ', 2)[0]);
                }
            }
        }

        return packages;
    }

    private static string PackageId(string assetKey)
    {
        var slashIndex = assetKey.LastIndexOf('/');
        return slashIndex < 0 ? assetKey : assetKey[..slashIndex];
    }

    private static void AddAssetAssemblies(
        HashSet<string> allowed,
        JsonElement library,
        string assetKind)
    {
        if (!library.TryGetProperty(assetKind, out var assets) ||
            assets.ValueKind != JsonValueKind.Object)
        {
            return;
        }

        foreach (var asset in assets.EnumerateObject())
        {
            if (asset.Name.EndsWith("/_._", StringComparison.Ordinal))
            {
                continue;
            }

            if (Path.GetExtension(asset.Name) == ".dll")
            {
                allowed.Add(Path.GetFileNameWithoutExtension(asset.Name));
            }
        }
    }
}
