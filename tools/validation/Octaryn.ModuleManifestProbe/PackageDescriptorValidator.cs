using Octaryn.Shared.GameModules;
using System.Text.Json;
using System.Text.Json.Nodes;

internal static class PackageDescriptorValidator
{
    public static List<string> Validate(string moduleRoot, GameModuleManifest manifest)
    {
        var errors = new List<string>();
        var descriptorPath = Path.Combine(moduleRoot, "Data", "Module", $"{manifest.ModuleId}.module.json");
        if (!File.Exists(descriptorPath))
        {
            errors.Add($"{manifest.ModuleId}: package descriptor missing at Data/Module/{manifest.ModuleId}.module.json");
            return errors;
        }

        var options = new JsonSerializerOptions
        {
            WriteIndented = true
        };
        var descriptor = JsonNode.Parse(File.ReadAllText(descriptorPath));
        var generated = JsonSerializer.SerializeToNode(manifest, options);
        if (!JsonNode.DeepEquals(descriptor, generated))
        {
            errors.Add($"{manifest.ModuleId}: package descriptor does not match code manifest");
        }

        return errors;
    }
}
