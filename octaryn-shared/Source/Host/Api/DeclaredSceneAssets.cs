using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal sealed class DeclaredSceneAssets
{
    private readonly string _root;
    private readonly Dictionary<string, string> _assets = new(StringComparer.Ordinal);

    public DeclaredSceneAssets(GameModuleManifest manifest, string moduleRoot)
    {
        _root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(moduleRoot));
        foreach (var asset in manifest.AssetDeclarations ?? [])
            if (asset.AssetKind is "model" or "scene" &&
                asset.AssetId.StartsWith(manifest.ModuleId + ".", StringComparison.Ordinal) && SafePath(asset.RelativePath))
                _assets.Add(asset.AssetId, asset.RelativePath);
    }

    internal bool TryResolve(string assetId, out string path, out string error)
    {
        path = string.Empty;
        error = "Scene asset is not declared by this module.";
        if (assetId is null || !_assets.TryGetValue(assetId, out var relative)) return false;
        try
        {
            var resolved = Path.GetFullPath(Path.Combine(_root, relative));
            var comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
            if (!resolved.StartsWith(_root + Path.DirectorySeparatorChar, comparison)) return false;
            var current = _root;
            RejectLink(current);
            foreach (var component in relative.Split('/'))
            {
                current = Path.Combine(current, component);
                RejectLink(current);
            }
            if (!File.Exists(resolved)) { error = "Declared scene source is unavailable."; return false; }
            path = resolved;
            error = string.Empty;
            return true;
        }
        catch (Exception failure) when (failure is IOException or UnauthorizedAccessException or ArgumentException)
        {
            error = "Declared scene source is unavailable: " + failure.Message;
            return false;
        }
    }

    private static bool SafePath(string path) => !string.IsNullOrWhiteSpace(path) &&
        path.StartsWith("Assets/", StringComparison.Ordinal) &&
        !path.Contains('\\') && !path.Contains(':') && !path.Contains('\0') && !Path.IsPathRooted(path) &&
        path.Split('/').All(part => part.Length > 0 && part is not "." and not ".." && !part.EndsWith('.') && !part.EndsWith(' '));

    private static void RejectLink(string path)
    {
        if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            throw new IOException("Scene source paths cannot traverse symbolic links or junctions.");
    }
}
