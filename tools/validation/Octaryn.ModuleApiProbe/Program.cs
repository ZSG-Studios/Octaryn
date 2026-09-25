return ModuleApiProbe.Run(args);

internal static partial class ModuleApiProbe
{
    public static int Run(IReadOnlyList<string> args)
    {
        var selfTestErrors = RunSelfTests();
        if (selfTestErrors.Count > 0)
        {
            foreach (var error in selfTestErrors)
            {
                Console.Error.WriteLine($"module API probe self-test: {error}");
            }

            return 1;
        }

        var moduleRoot = ParseSourceRoot(args);
        var assetsPath = ParseOption(args, "--assets-file");
        var errors = Validate(moduleRoot, assetsPath);
        if (errors.Count == 0)
        {
            return 0;
        }

        foreach (var error in errors)
        {
            Console.Error.WriteLine($"module API probe: {error}");
        }

        return 1;
    }

    private static string ParseSourceRoot(IReadOnlyList<string> args)
    {
        for (var index = 0; index < args.Count; index++)
        {
            if (args[index] == "--source-root" && index + 1 < args.Count)
            {
                return Path.GetFullPath(args[index + 1]);
            }
        }

        for (var index = 0; index < args.Count; index++)
        {
            if (args[index].StartsWith("--", StringComparison.Ordinal))
            {
                index++;
                continue;
            }

            return Path.GetFullPath(args[index]);
        }

        return Path.GetFullPath("octaryn-basegame");
    }

    private static string? ParseOption(IReadOnlyList<string> args, string name)
    {
        for (var index = 0; index < args.Count - 1; index++)
        {
            if (args[index] == name)
            {
                return Path.GetFullPath(args[index + 1]);
            }
        }

        return null;
    }
}
