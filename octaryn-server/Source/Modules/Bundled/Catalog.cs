using System.Text.Json;
using Octaryn.Shared.GameModules;

namespace Octaryn.Server.Modules.Bundled;

internal static class Catalog
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
        Path.GetDirectoryName(typeof(Catalog).Assembly.Location) ?? AppContext.BaseDirectory,
        "Data",
        "Module");
}
