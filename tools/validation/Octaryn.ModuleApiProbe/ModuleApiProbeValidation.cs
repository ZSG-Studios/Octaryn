using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.ModuleSandbox;

internal static partial class ModuleApiProbe
{
    private static List<string> Validate(string moduleRoot, string? assetsPath = null)
    {
        var errors = new List<string>();
        var requestedGroups = LoadRequestedFrameworkGroups(moduleRoot, assetsPath, errors);

        foreach (var deniedGroup in DeniedFrameworkApiGroups.Values)
        {
            if (requestedGroups.Contains(deniedGroup))
            {
                errors.Add($"module manifest requests denied framework API group {deniedGroup}");
            }
        }

        foreach (var requestedGroup in requestedGroups)
        {
            if (!FrameworkApiGroupAllowlist.IsAllowed(requestedGroup))
            {
                errors.Add($"module manifest requests unapproved framework API group {requestedGroup}");
            }
        }

        var sourceRoot = Path.Combine(moduleRoot, "Source");
        var sourceFiles = Directory.EnumerateFiles(sourceRoot, "*.cs", SearchOption.AllDirectories)
            .Where(path => !path.Split(Path.DirectorySeparatorChar).Contains("bin") &&
                !path.Split(Path.DirectorySeparatorChar).Contains("obj"))
            .Order(StringComparer.Ordinal)
            .ToArray();
        var parseOptions = CSharpParseOptions.Default.WithLanguageVersion(LanguageVersion.Latest);
        var syntaxTrees = sourceFiles
            .Select(path => CSharpSyntaxTree.ParseText(File.ReadAllText(path), parseOptions, path))
            .ToArray();
        var compilation = CSharpCompilation.Create(
            "Octaryn.ModuleApiProbe.Input",
            syntaxTrees,
            CompilationReferences(moduleRoot, assetsPath),
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

        foreach (var tree in syntaxTrees)
        {
            var semanticModel = compilation.GetSemanticModel(tree, ignoreAccessibility: true);
            var root = tree.GetCompilationUnitRoot();
            ValidateUsingDirectives(errors, tree, root, requestedGroups);
            ValidateAllowedSyntax(errors, tree, root, requestedGroups);
            ValidateAttributes(errors, tree, root);
            ValidateReflectionSyntax(errors, tree, root);
            ValidateUnsafeSyntax(errors, tree, root);
            ValidateThreadingSyntax(errors, tree, root);
            ValidateIdentifierSymbols(errors, tree, semanticModel, root, requestedGroups);
        }

        foreach (var diagnostic in compilation.GetDiagnostics().Where(diagnostic => diagnostic.Severity == DiagnosticSeverity.Error))
        {
            errors.Add($"{diagnostic.Location.GetLineSpan().Path}:{diagnostic.Location.GetLineSpan().StartLinePosition.Line + 1}: compile error {diagnostic.Id}: {diagnostic.GetMessage()}");
        }

        return errors;
    }

    private static void ValidateUsingDirectives(
        List<string> errors,
        SyntaxTree tree,
        CompilationUnitSyntax root,
        IReadOnlySet<string> requestedGroups)
    {
        foreach (var usingDirective in root.DescendantNodes().OfType<UsingDirectiveSyntax>())
        {
            var name = NormalizeName(usingDirective.Name);
            if (name is null)
            {
                continue;
            }

            if (FindDeniedGroup(name) is { } group)
            {
                errors.Add($"{Location(tree, usingDirective.GetLocation())}: denied framework API group {group}: using {name}");
                continue;
            }

            if (IsDeniedModuleApiNamespace(name))
            {
                errors.Add($"{Location(tree, usingDirective.GetLocation())}: denied module API namespace: using {name}");
                continue;
            }

            if (FindAllowedGroup(name) is { } allowedGroup)
            {
                if (!requestedGroups.Contains(allowedGroup))
                {
                    errors.Add($"{Location(tree, usingDirective.GetLocation())}: unrequested framework API group {allowedGroup}: using {name}");
                }

                continue;
            }

            if (IsSystemApi(name))
            {
                errors.Add($"{Location(tree, usingDirective.GetLocation())}: unclassified framework API: using {name}");
            }
        }
    }

    private static void ValidateAttributes(
        List<string> errors,
        SyntaxTree tree,
        CompilationUnitSyntax root)
    {
        foreach (var attribute in root.DescendantNodes().OfType<AttributeSyntax>())
        {
            var name = NormalizeName(attribute.Name);
            if (name is "DllImport" or "DllImportAttribute" or "LibraryImport" or "LibraryImportAttribute")
            {
                errors.Add($"{Location(tree, attribute.GetLocation())}: denied framework API group {FrameworkApiGroupIds.BclNativeInterop}: {name}");
            }
        }
    }

    private static void ValidateAllowedSyntax(
        List<string> errors,
        SyntaxTree tree,
        CompilationUnitSyntax root,
        IReadOnlySet<string> requestedGroups)
    {
        foreach (var memberAccess in root.DescendantNodes().OfType<MemberAccessExpressionSyntax>())
        {
            var expression = memberAccess.Expression.ToString()
                .Replace("global::", string.Empty, StringComparison.Ordinal);
            if (AllowedTypeNames.TryGetValue(expression.Split('.').Last(), out var group) &&
                !requestedGroups.Contains(group))
            {
                errors.Add($"{Location(tree, memberAccess.GetLocation())}: unrequested framework API group {group}: {expression}");
            }
        }
    }

    private static void ValidateReflectionSyntax(
        List<string> errors,
        SyntaxTree tree,
        CompilationUnitSyntax root)
    {
        foreach (var node in root.DescendantNodes().OfType<TypeOfExpressionSyntax>())
        {
            errors.Add($"{Location(tree, node.GetLocation())}: denied framework API group {FrameworkApiGroupIds.BclReflection}: typeof");
        }

        foreach (var invocation in root.DescendantNodes().OfType<InvocationExpressionSyntax>())
        {
            if (invocation.Expression is MemberAccessExpressionSyntax memberAccess &&
                memberAccess.Name.Identifier.ValueText == "GetType")
            {
                errors.Add($"{Location(tree, invocation.GetLocation())}: denied framework API group {FrameworkApiGroupIds.BclReflection}: GetType");
            }
        }
    }

    private static void ValidateUnsafeSyntax(
        List<string> errors,
        SyntaxTree tree,
        CompilationUnitSyntax root)
    {
        foreach (var node in root.DescendantNodes())
        {
            if (node is UnsafeStatementSyntax or FixedStatementSyntax or StackAllocArrayCreationExpressionSyntax or FunctionPointerTypeSyntax)
            {
                errors.Add($"{Location(tree, node.GetLocation())}: denied framework API group {FrameworkApiGroupIds.BclUnsafeCode}: {node.Kind()}");
            }
        }

        foreach (var token in root.DescendantTokens())
        {
            if (token.IsKind(SyntaxKind.UnsafeKeyword))
            {
                errors.Add($"{Location(tree, token.GetLocation())}: denied framework API group {FrameworkApiGroupIds.BclUnsafeCode}: unsafe");
            }
        }
    }

    private static void ValidateThreadingSyntax(
        List<string> errors,
        SyntaxTree tree,
        CompilationUnitSyntax root)
    {
        foreach (var node in root.DescendantNodes().OfType<LockStatementSyntax>())
        {
            errors.Add($"{Location(tree, node.GetLocation())}: denied framework API group {FrameworkApiGroupIds.BclThreading}: lock");
        }
    }

    private static void ValidateIdentifierSymbols(
        List<string> errors,
        SyntaxTree tree,
        SemanticModel semanticModel,
        CompilationUnitSyntax root,
        IReadOnlySet<string> requestedGroups)
    {
        foreach (var node in root.DescendantNodes())
        {
            if (node is IdentifierNameSyntax identifier)
            {
                ValidateSymbol(errors, tree, semanticModel, identifier, identifier.Identifier.ValueText, requestedGroups);
                continue;
            }

            if (node is MemberAccessExpressionSyntax memberAccess)
            {
                ValidateSymbol(errors, tree, semanticModel, memberAccess, memberAccess.Name.Identifier.ValueText, requestedGroups);
            }
        }
    }

    private static void ValidateSymbol(
        List<string> errors,
        SyntaxTree tree,
        SemanticModel semanticModel,
        SyntaxNode node,
        string fallbackName,
        IReadOnlySet<string> requestedGroups)
    {
        var symbol = semanticModel.GetSymbolInfo(node).Symbol ??
            semanticModel.GetTypeInfo(node).Type;
        var containingType = symbol switch
        {
            IMethodSymbol method => method.ContainingType,
            IPropertySymbol property => property.ContainingType,
            IFieldSymbol field => field.ContainingType,
            INamedTypeSymbol namedType => namedType,
            _ => null
        };

        if (containingType is not null)
        {
            var fullyQualifiedName = containingType.ToDisplayString(SymbolDisplayFormat.FullyQualifiedFormat)
                .Replace("global::", string.Empty, StringComparison.Ordinal);
            if (FindDeniedGroup(fullyQualifiedName) is { } group)
            {
                errors.Add($"{Location(tree, node.GetLocation())}: denied framework API group {group}: {fullyQualifiedName}");
                return;
            }

            if (DeniedHostControlTypes.Contains(fullyQualifiedName))
            {
                errors.Add($"{Location(tree, node.GetLocation())}: denied host control API: {fullyQualifiedName}");
                return;
            }

            if (IsDeniedModuleApiNamespace(fullyQualifiedName))
            {
                errors.Add($"{Location(tree, node.GetLocation())}: denied module API namespace: {fullyQualifiedName}");
                return;
            }

            var allowedGroup = FindAllowedGroup(fullyQualifiedName) ?? FindAllowedTypeGroup(fullyQualifiedName);
            if (allowedGroup is not null)
            {
                if (!requestedGroups.Contains(allowedGroup))
                {
                    errors.Add($"{Location(tree, node.GetLocation())}: unrequested framework API group {allowedGroup}: {fullyQualifiedName}");
                }

                return;
            }

            if (IsSystemApi(fullyQualifiedName))
            {
                errors.Add($"{Location(tree, node.GetLocation())}: unclassified framework API: {fullyQualifiedName}");
            }

            return;
        }

        if (DeniedTypeNames.TryGetValue(fallbackName, out var fallbackGroup))
        {
            errors.Add($"{Location(tree, node.GetLocation())}: denied framework API group {fallbackGroup}: {fallbackName}");
            return;
        }

        if (AllowedTypeNames.TryGetValue(fallbackName, out var allowedFallbackGroup) &&
            !requestedGroups.Contains(allowedFallbackGroup))
        {
            errors.Add($"{Location(tree, node.GetLocation())}: unrequested framework API group {allowedFallbackGroup}: {fallbackName}");
        }
    }

    private static string? NormalizeName(NameSyntax? name)
    {
        return name?.ToString()
            .Replace("global::", string.Empty, StringComparison.Ordinal)
            .Trim();
    }

    private static string Location(SyntaxTree tree, Location location)
    {
        var span = location.GetLineSpan();
        return $"{tree.FilePath}:{span.StartLinePosition.Line + 1}";
    }
}
