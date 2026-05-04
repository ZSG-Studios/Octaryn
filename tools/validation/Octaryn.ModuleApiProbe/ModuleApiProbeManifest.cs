using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Octaryn.Shared.GameModules;
using System.Text.Json;

internal static partial class ModuleApiProbe
{
    private static HashSet<string> LoadRequestedFrameworkGroups(string moduleRoot, string? assetsPath, List<string> errors)
    {
        var groups = new HashSet<string>(StringComparer.Ordinal);
        var sourceRoot = Path.Combine(moduleRoot, "Source");
        var parseOptions = CSharpParseOptions.Default.WithLanguageVersion(LanguageVersion.Latest);
        var syntaxTrees = new List<SyntaxTree>();
        foreach (var path in Directory.EnumerateFiles(sourceRoot, "*.cs", SearchOption.AllDirectories))
        {
            if (path.Split(Path.DirectorySeparatorChar).Contains("bin") ||
                path.Split(Path.DirectorySeparatorChar).Contains("obj"))
            {
                continue;
            }

            var text = File.ReadAllText(path);
            syntaxTrees.Add(CSharpSyntaxTree.ParseText(text, parseOptions, path));
        }

        var compilation = CSharpCompilation.Create(
            "Octaryn.ModuleApiProbe.ManifestInput",
            syntaxTrees,
            CompilationReferences(moduleRoot, assetsPath),
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

        foreach (var tree in syntaxTrees)
        {
            var path = tree.FilePath;
            var root = tree.GetCompilationUnitRoot();
            var semanticModel = compilation.GetSemanticModel(tree, ignoreAccessibility: true);
            foreach (var manifestCreation in ManifestCreations(root, semanticModel))
            {
                LoadFrameworkGroupsFromManifestCreation(path, errors, groups, manifestCreation);
            }
        }

        if (groups.Count == 0)
        {
            errors.Add($"{moduleRoot}: no requested framework API groups found in module source.");
        }

        return groups;
    }

    private static IEnumerable<BaseObjectCreationExpressionSyntax> ManifestCreations(
        CompilationUnitSyntax root,
        SemanticModel semanticModel)
    {
        foreach (var creation in root.DescendantNodes().OfType<ObjectCreationExpressionSyntax>())
        {
            if (IsGameModuleManifestCreation(semanticModel, creation))
            {
                yield return creation;
            }
        }

        foreach (var implicitCreation in root.DescendantNodes().OfType<ImplicitObjectCreationExpressionSyntax>())
        {
            if (IsGameModuleManifestCreation(semanticModel, implicitCreation))
            {
                yield return implicitCreation;
            }
        }
    }

    private static bool IsGameModuleManifestCreation(
        SemanticModel semanticModel,
        BaseObjectCreationExpressionSyntax creation)
    {
        return semanticModel.GetTypeInfo(creation).Type?.ToDisplayString(SymbolDisplayFormat.FullyQualifiedFormat) ==
            "global::Octaryn.Shared.GameModules.GameModuleManifest";
    }

    private static void LoadFrameworkGroupsFromManifestCreation(
        string path,
        List<string> errors,
        HashSet<string> groups,
        BaseObjectCreationExpressionSyntax manifestCreation)
    {
        foreach (var argument in manifestCreation.ArgumentList?.Arguments ?? [])
        {
            if (argument.NameColon?.Name.Identifier.ValueText != "RequestedFrameworkApiGroups")
            {
                continue;
            }

            foreach (var memberAccess in argument.Expression.DescendantNodesAndSelf().OfType<MemberAccessExpressionSyntax>())
            {
                if (!memberAccess.ToString().Contains("FrameworkApiGroupIds.", StringComparison.Ordinal))
                {
                    continue;
                }

                var value = ResolveFrameworkGroup(memberAccess.Name.Identifier.ValueText);
                if (value is null)
                {
                    errors.Add($"{path}: unknown FrameworkApiGroupIds constant {memberAccess.Name.Identifier.ValueText}");
                    continue;
                }

                groups.Add(value);
            }

            foreach (var literal in argument.Expression.DescendantNodesAndSelf().OfType<LiteralExpressionSyntax>())
            {
                if (!literal.IsKind(SyntaxKind.StringLiteralExpression))
                {
                    continue;
                }

                var value = literal.Token.ValueText;
                if (value.StartsWith("bcl.", StringComparison.Ordinal))
                {
                    groups.Add(value);
                }
            }
        }
    }


    private static IEnumerable<MetadataReference> CompilationReferences(string moduleRoot, string? assetsPath)
    {
        var seen = new HashSet<string>(StringComparer.Ordinal);
        var paths = ((string?)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") ?? string.Empty)
            .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries);
        foreach (var path in paths)
        {
            if (seen.Add(path))
            {
                yield return MetadataReference.CreateFromFile(path);
            }
        }

        if (seen.Add(typeof(GameModuleManifest).Assembly.Location))
        {
            yield return MetadataReference.CreateFromFile(typeof(GameModuleManifest).Assembly.Location);
        }

        foreach (var path in ModulePackageReferencePaths(moduleRoot, assetsPath))
        {
            if (seen.Add(path))
            {
                yield return MetadataReference.CreateFromFile(path);
            }
        }
    }

    private static IEnumerable<string> ModulePackageReferencePaths(string moduleRoot, string? assetsPath)
    {
        assetsPath ??= ResolveProjectAssetsPath(moduleRoot);
        if (assetsPath is null)
        {
            yield break;
        }

        using var document = JsonDocument.Parse(File.ReadAllText(assetsPath));
        var packageFolders = document.RootElement.GetProperty("packageFolders")
            .EnumerateObject()
            .Select(folder => folder.Name)
            .ToArray();

        foreach (var target in document.RootElement.GetProperty("targets").EnumerateObject())
        {
            foreach (var package in target.Value.EnumerateObject())
            {
                if (!package.Name.Contains('/', StringComparison.Ordinal))
                {
                    continue;
                }

                var packageParts = package.Name.Split('/', 2);
                foreach (var assetGroupName in new[] { "compile", "runtime" })
                {
                    if (!package.Value.TryGetProperty(assetGroupName, out var assets))
                    {
                        continue;
                    }

                    foreach (var asset in assets.EnumerateObject())
                    {
                        if (!asset.Name.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ||
                            asset.Name.EndsWith("/_._", StringComparison.Ordinal))
                        {
                            continue;
                        }

                        foreach (var packageFolder in packageFolders)
                        {
                            var path = Path.Combine(packageFolder, packageParts[0].ToLowerInvariant(), packageParts[1], asset.Name);
                            if (File.Exists(path))
                            {
                                yield return path;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    private static string? ResolveProjectAssetsPath(string moduleRoot)
    {
        var projectFile = Directory.EnumerateFiles(moduleRoot, "*.csproj", SearchOption.TopDirectoryOnly)
            .Order(StringComparer.Ordinal)
            .FirstOrDefault();
        if (projectFile is null)
        {
            return null;
        }

        var repoRoot = FindRepoRoot(moduleRoot);
        var candidate = Path.Combine(repoRoot, "build", "debug-linux", "basegame", "managed-obj", "project.assets.json");
        if (File.Exists(candidate))
        {
            return candidate;
        }

        return null;
    }

    private static string FindRepoRoot(string moduleRoot)
    {
        var directory = new DirectoryInfo(moduleRoot);
        while (directory is not null)
        {
            if (File.Exists(Path.Combine(directory.FullName, "Directory.Build.props")) &&
                Directory.Exists(Path.Combine(directory.FullName, "tools", "validation")))
            {
                return directory.FullName;
            }

            directory = directory.Parent;
        }

        return Directory.GetCurrentDirectory();
    }
}
