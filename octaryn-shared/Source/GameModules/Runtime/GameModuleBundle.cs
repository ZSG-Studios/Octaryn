namespace Octaryn.Shared.GameModules;

public static class GameModuleBundle
{
    public const string RootEnvironmentVariable = "OCTARYN_GAME_MODULE_ROOT";

    public static string ResolveRoot(string hostDirectory)
    {
        var selected = Environment.GetEnvironmentVariable(RootEnvironmentVariable);
        var root = Path.GetFullPath(string.IsNullOrWhiteSpace(selected) ? ResolveHostDirectory(hostDirectory) : selected);
        if (!Directory.Exists(root))
            throw new InvalidOperationException($"Game module bundle does not exist: {root}");
        return root;
    }

    internal static string ResolveHostDirectory(string? hostDirectory)
    {
        if (!string.IsNullOrWhiteSpace(hostDirectory)) return hostDirectory;
        var assemblyDirectory = Path.GetDirectoryName(typeof(GameModuleBundle).Assembly.Location);
        if (!string.IsNullOrWhiteSpace(assemblyDirectory)) return assemblyDirectory;
        var process = Environment.ProcessPath;
        var processDirectory = string.IsNullOrWhiteSpace(process) ? null : Path.GetDirectoryName(process);
        if (!string.IsNullOrWhiteSpace(processDirectory)) return processDirectory;
        throw new InvalidOperationException("Cannot determine the host bundle directory from the loaded assembly or executable.");
    }

    public static string ResolveManifestPath(string root)
    {
        var directory = Path.Combine(root, "Data", "Module");
        var manifests = Directory.EnumerateFiles(directory, "*.module.json").Order().Take(2).ToArray();
        if (manifests.Length != 1)
            throw new InvalidOperationException($"Expected exactly one game module manifest in {directory}.");
        return manifests[0];
    }
}
