using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal sealed class DeclaredContentApi : IHostContentApi
{
    private readonly string _root;
    private readonly Dictionary<string, string> _declarations = new(StringComparer.Ordinal);

    public DeclaredContentApi(GameModuleManifest manifest, string moduleRoot)
    {
        _root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(moduleRoot));
        foreach (var declaration in manifest.ContentDeclarations ?? [])
        {
            if (declaration.ContentId.StartsWith(manifest.ModuleId + ".", StringComparison.Ordinal) &&
                SafePath(declaration.RelativePath))
                _declarations.Add(declaration.ContentId, declaration.RelativePath);
        }
    }

    internal bool Contains(string contentId) => contentId is not null && _declarations.ContainsKey(contentId);

    public bool TryRead(string contentId, out ReadOnlyMemory<byte> data, out string error)
    {
        data = default;
        error = "Content ID is not declared by this module.";
        if (contentId is null || !_declarations.TryGetValue(contentId, out var relative)) return false;
        try
        {
            var path = Path.GetFullPath(Path.Combine(_root, relative));
            var comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
            if (!path.StartsWith(_root + Path.DirectorySeparatorChar, comparison)) return false;
            var current = _root;
            RejectLink(current);
            foreach (var component in relative.Split('/'))
            {
                current = Path.Combine(current, component);
                RejectLink(current);
            }
            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            if (stream.Length > IHostContentApi.MaximumReadBytes)
            {
                error = "Declared content exceeds the 4 MiB read limit.";
                return false;
            }
            var bytes = new byte[checked((int)stream.Length)];
            stream.ReadExactly(bytes);
            if (stream.ReadByte() != -1)
            {
                error = "Declared content changed while reading.";
                return false;
            }
            data = bytes;
            error = string.Empty;
            return true;
        }
        catch (Exception failure) when (failure is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException)
        {
            error = "Declared content is unavailable: " + failure.Message;
            return false;
        }
    }

    private static bool SafePath(string path) => !string.IsNullOrWhiteSpace(path) &&
        path.StartsWith("Data/", StringComparison.Ordinal) &&
        !path.Contains('\\') && !path.Contains(':') && !path.Contains('\0') &&
        !Path.IsPathRooted(path) && path.Split('/').All(part => part.Length > 0 &&
            part is not "." and not ".." && !part.EndsWith('.') && !part.EndsWith(' '));

    private static void RejectLink(string path)
    {
        if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            throw new IOException("Declared content paths cannot traverse symbolic links or junctions.");
    }
}
