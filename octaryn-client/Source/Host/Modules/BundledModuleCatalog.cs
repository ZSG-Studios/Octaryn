using System.Text.Json;
using Octaryn.Shared.GameModules;

namespace Octaryn.Client.Host;

internal static class BundledModuleCatalog
{
    public static GameModuleManifest? ResolveManifest(string moduleId)
    {
        var path = Path.Combine(ModuleDirectory, $"{moduleId}.module.json");
        if (!File.Exists(path))
        {
            return null;
        }

        return JsonSerializer.Deserialize<GameModuleManifest>(File.ReadAllText(path));
    }

    private static string ModuleDirectory => Path.Combine(
        Path.GetDirectoryName(typeof(BundledModuleCatalog).Assembly.Location) ?? AppContext.BaseDirectory,
        "Data",
        "Module");
}
