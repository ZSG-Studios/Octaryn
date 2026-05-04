using Octaryn.Shared.GameModules;
using System.Text.Json;

internal static class ManifestValidator
{
    public static List<string> Validate(string moduleRoot, GameModuleManifest manifest)
    {
        var errors = new List<string>();
        var report = GameModuleValidator.Validate(manifest);
        errors.AddRange(
            report.Issues
                .Where(issue => issue.Severity == ModuleValidationSeverity.Error)
                .Select(issue => issue.Message));

        var declaredContent = new HashSet<string>(StringComparer.Ordinal);
        foreach (var content in manifest.ContentDeclarations)
        {
            ValidateDeclaredFile(
                errors,
                moduleRoot,
                declaredContent,
                content.ContentId,
                content.ContentKind,
                content.RelativePath,
                ["Data/"]);
        }

        var declaredAssets = new HashSet<string>(StringComparer.Ordinal);
        foreach (var asset in manifest.AssetDeclarations)
        {
            ValidateDeclaredFile(
                errors,
                moduleRoot,
                declaredAssets,
                asset.AssetId,
                null,
                asset.RelativePath,
                ["Assets/", "Shaders/"]);
        }

        ValidateUndeclaredFiles(errors, moduleRoot, "Data", declaredContent, "content");
        ValidateUndeclaredFiles(errors, moduleRoot, "Assets", declaredAssets, "asset");
        ValidateUndeclaredFiles(errors, moduleRoot, "Shaders", declaredAssets, "shader");
        return errors;
    }

    private static void ValidateDeclaredFile(
        List<string> errors,
        string moduleRoot,
        HashSet<string> declaredPaths,
        string declarationId,
        string? declarationKind,
        string relativePath,
        IReadOnlyList<string> allowedPrefixes)
    {
        if (Path.IsPathRooted(relativePath) ||
            relativePath.Contains("..", StringComparison.Ordinal) ||
            relativePath.Contains(':', StringComparison.Ordinal))
        {
            errors.Add($"{declarationId}: unsafe relative path {relativePath}");
            return;
        }

        if (!allowedPrefixes.Any(prefix => relativePath.StartsWith(prefix, StringComparison.Ordinal)))
        {
            errors.Add($"{declarationId}: path {relativePath} is outside allowed module roots.");
            return;
        }

        declaredPaths.Add(relativePath);
        var moduleFullPath = Path.GetFullPath(moduleRoot);
        var path = Path.GetFullPath(Path.Combine(moduleFullPath, relativePath));
        if (!path.StartsWith(moduleFullPath + Path.DirectorySeparatorChar, StringComparison.Ordinal))
        {
            errors.Add($"{declarationId}: path escapes module root {relativePath}");
            return;
        }

        if (!File.Exists(path))
        {
            errors.Add($"{declarationId}: declared file missing at {relativePath}");
            return;
        }

        if (new FileInfo(path).Length == 0)
        {
            errors.Add($"{declarationId}: declared file is empty at {relativePath}");
            return;
        }

        if (declarationKind is not null && Path.GetExtension(path).Equals(".json", StringComparison.OrdinalIgnoreCase))
        {
            ValidateDeclaredJsonContentIdentity(errors, path, declarationId, declarationKind);
        }
    }

    private static void ValidateDeclaredJsonContentIdentity(
        List<string> errors,
        string path,
        string declarationId,
        string declarationKind)
    {
        JsonDocument document;
        try
        {
            document = JsonDocument.Parse(File.ReadAllText(path));
        }
        catch (JsonException error)
        {
            errors.Add($"{declarationId}: declared JSON content is invalid at {Path.GetFileName(path)}: {error.Message}");
            return;
        }

        using (document)
        {
            if (document.RootElement.ValueKind != JsonValueKind.Object)
            {
                errors.Add($"{declarationId}: declared JSON content must be an object at {Path.GetFileName(path)}");
                return;
            }

            if (!document.RootElement.TryGetProperty("id", out var idElement) ||
                idElement.ValueKind != JsonValueKind.String ||
                !string.Equals(idElement.GetString(), declarationId, StringComparison.Ordinal))
            {
                errors.Add($"{declarationId}: declared content id mismatch at {Path.GetFileName(path)}");
            }

            if (!document.RootElement.TryGetProperty("kind", out var kindElement) ||
                kindElement.ValueKind != JsonValueKind.String ||
                !string.Equals(kindElement.GetString(), declarationKind, StringComparison.Ordinal))
            {
                errors.Add($"{declarationId}: declared content kind mismatch at {Path.GetFileName(path)}");
            }
        }
    }

    private static void ValidateUndeclaredFiles(
        List<string> errors,
        string moduleRoot,
        string directoryName,
        HashSet<string> declaredPaths,
        string fileKind)
    {
        var directory = Path.Combine(moduleRoot, directoryName);
        if (!Directory.Exists(directory))
        {
            errors.Add($"{moduleRoot}: missing {directoryName}/ directory");
            return;
        }

        foreach (var file in Directory.EnumerateFiles(directory, "*", SearchOption.AllDirectories))
        {
            if (Path.GetFileName(file) == ".gitkeep")
            {
                continue;
            }

            var relativePath = Path.GetRelativePath(moduleRoot, file).Replace('\\', '/');
            if (fileKind == "content" &&
                relativePath.StartsWith("Data/Module/", StringComparison.Ordinal) &&
                relativePath.EndsWith(".module.json", StringComparison.Ordinal))
            {
                continue;
            }

            if (!declaredPaths.Contains(relativePath))
            {
                errors.Add($"{relativePath}: undeclared {fileKind} file");
            }
        }
    }
}
