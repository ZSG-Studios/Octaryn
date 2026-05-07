return ServerPersistenceProbe.Run();

internal static partial class ServerPersistenceProbe
{
    public static int Run()
    {
        ValidatePlayerFileRoundTrip();
        ValidatePlayerPersistenceRoot();
        ValidateChunkColumnOverrideFiles();
        ValidateWorldSaveMetadata();
        ValidateServerSaveExportBundle();
        return 0;
    }

    private static string ResetProbeDirectory(string name)
    {
        var root = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PERSISTENCE_PROBE_DIR");
        if (string.IsNullOrWhiteSpace(root))
        {
            var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
            if (string.IsNullOrWhiteSpace(presetName))
            {
                presetName = "debug-linux";
            }

            root = Path.Combine("build", presetName, "server", "validation", "server-persistence");
        }

        var directory = Path.Combine(root, name);
        if (Directory.Exists(directory))
        {
            Directory.Delete(directory, recursive: true);
        }

        Directory.CreateDirectory(directory);
        return directory;
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }
}
