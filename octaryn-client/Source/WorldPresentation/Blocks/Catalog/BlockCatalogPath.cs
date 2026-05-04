namespace Octaryn.Client.WorldPresentation;

internal static class BlockCatalogPath
{
    public static string Resolve()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_CLIENT_BLOCK_CATALOG_PATH");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var assemblyDirectory = Path.GetDirectoryName(typeof(BlockCatalogPath).Assembly.Location);
        var blockDirectory = Path.Combine(assemblyDirectory ?? AppContext.BaseDirectory, "Data", "Blocks");
        var bundledPath = Directory.Exists(blockDirectory)
            ? Directory.EnumerateFiles(blockDirectory, "*.blocks.json").Order().FirstOrDefault()
            : null;
        if (!string.IsNullOrWhiteSpace(bundledPath))
        {
            return bundledPath;
        }

        throw new InvalidOperationException($"No bundled block catalog was found in {blockDirectory}.");
    }
}
