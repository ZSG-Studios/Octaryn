using Octaryn.Shared.GameModules;
using System.Text.Json;

internal static class ManifestDumpWriter
{
    public static void Write(GameModuleManifest manifest, string path)
    {
        var fullPath = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
        var options = new JsonSerializerOptions
        {
            WriteIndented = true
        };
        File.WriteAllText(fullPath, JsonSerializer.Serialize(manifest, options));
    }
}
