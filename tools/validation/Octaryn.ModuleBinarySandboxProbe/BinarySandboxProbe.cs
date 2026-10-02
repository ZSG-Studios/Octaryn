using System;
using System.Collections.Generic;
using System.IO;

internal static class BinarySandboxProbe
{
    public static int Run(IReadOnlyList<string> args)
    {
        var selfTestErrors = SelfTests.Run();
        if (selfTestErrors.Count > 0)
        {
            foreach (var error in selfTestErrors)
            {
                Console.Error.WriteLine($"module binary sandbox probe self-test: {error}");
            }

            return 1;
        }

        var assemblyPath = ParseAssemblyPath(args);
        if (assemblyPath is null)
        {
            Console.Error.WriteLine("module binary sandbox probe: --assembly <path> is required");
            return 1;
        }

        var assetsPath = ParseOption(args, "--assets-file");
        var policyPath = ParseOption(args, "--policy-file");
        var sharedProject = ParseOption(args, "--shared-project");
        var libraries = ParseOptions(args,"--library-project");
        var errors = AssemblyValidator.Validate(assemblyPath, assetsPath, policyPath, sharedProject,libraries);
        foreach(var project in libraries)
        {
            // Validate every trusted library's compiled code under the same API restrictions.
            var libraryAssembly=Path.Combine(Path.GetDirectoryName(assemblyPath)!,Path.GetFileNameWithoutExtension(project)+".dll");
            if(assetsPath is null)errors.Add("Owned library validation requires --assets-file.");
            errors.AddRange(AssemblyValidator.Validate(libraryAssembly,assetsPath,policyPath,sharedProject,libraries));
        }
        if (errors.Count == 0)
        {
            return 0;
        }

        foreach (var error in errors)
        {
            Console.Error.WriteLine($"module binary sandbox probe: {error}");
        }

        return 1;
    }

    private static string? ParseAssemblyPath(IReadOnlyList<string> args)
    {
        for (var index = 0; index < args.Count; index++)
        {
            if (args[index] == "--assembly" && index + 1 < args.Count)
            {
                return Path.GetFullPath(args[index + 1]);
            }
        }

        return args.Count > 0 ? Path.GetFullPath(args[0]) : null;
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
    private static IReadOnlyList<string> ParseOptions(IReadOnlyList<string> args,string name)
    {
        var result=new List<string>();
        for(var i=0;i<args.Count-1;++i)if(args[i]==name)result.Add(Path.GetFullPath(args[++i]));
        return result;
    }
}
