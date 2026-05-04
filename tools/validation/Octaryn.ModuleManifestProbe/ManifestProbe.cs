using Octaryn.Basegame.Module;

internal static class ManifestProbe
{
    public static int Run(IReadOnlyList<string> args)
    {
        var selfTestErrors = SelfTests.Run();
        if (selfTestErrors.Count > 0)
        {
            foreach (var error in selfTestErrors)
            {
                Console.Error.WriteLine($"module manifest probe self-test: {error}");
            }

            return 1;
        }

        var moduleRootArgument = ModuleRootArgument(args);
        var moduleRoot = moduleRootArgument is not null
            ? Path.GetFullPath(moduleRootArgument)
            : Path.GetFullPath("octaryn-basegame");
        var registration = new BasegameModuleRegistration();
        var errors = ManifestValidator.Validate(moduleRoot, registration.Manifest);
        errors.AddRange(PackageDescriptorValidator.Validate(moduleRoot, registration.Manifest));
        if (errors.Count == 0)
        {
            var dumpPath = ArgumentValue(args, "--dump-manifest");
            if (dumpPath is not null)
            {
                ManifestDumpWriter.Write(registration.Manifest, dumpPath);
            }

            return 0;
        }

        foreach (var error in errors)
        {
            Console.Error.WriteLine($"module manifest probe: {error}");
        }

        return 1;
    }

    private static string? ArgumentValue(IReadOnlyList<string> args, string name)
    {
        for (var index = 0; index < args.Count - 1; index++)
        {
            if (args[index] == name)
            {
                return args[index + 1];
            }
        }

        return null;
    }

    private static string? ModuleRootArgument(IReadOnlyList<string> args)
    {
        for (var index = 0; index < args.Count; index++)
        {
            if (args[index].StartsWith("--", StringComparison.Ordinal))
            {
                index++;
                continue;
            }

            return args[index];
        }

        return null;
    }
}
