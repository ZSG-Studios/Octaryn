using System.Text.Json;
using System.Text.Json.Nodes;

namespace Octaryn.Shared.GameModules;

public static class BundledModuleMetadataVerifier
{
    public static bool Matches(GameModuleManifest bundled, GameModuleManifest registration)
    {
        return JsonNode.DeepEquals(
            JsonSerializer.SerializeToNode(bundled),
            JsonSerializer.SerializeToNode(registration));
    }
}
