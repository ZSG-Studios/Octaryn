namespace Octaryn.Client.WorldPresentation;

internal static class ClientBasegameBlockCatalogPath
{
    private const string CatalogRelativePath = "Data/Blocks/octaryn.basegame.blocks.json";

    public static string Resolve()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_CLIENT_BLOCK_CATALOG_PATH");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var bundledPath = Path.Combine(AppContext.BaseDirectory, CatalogRelativePath);
        if (File.Exists(bundledPath))
        {
            return bundledPath;
        }

        return bundledPath;
    }
}
