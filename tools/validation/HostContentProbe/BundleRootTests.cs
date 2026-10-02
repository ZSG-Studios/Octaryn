using Octaryn.Shared.GameModules;

internal static unsafe partial class Program
{
    private static void VerifyBundleRoot(string root)
    {
        var selected = Environment.GetEnvironmentVariable(GameModuleBundle.RootEnvironmentVariable);
        var workingDirectory = Environment.CurrentDirectory;
        try
        {
            Environment.SetEnvironmentVariable(GameModuleBundle.RootEnvironmentVariable, null);
            Environment.CurrentDirectory = root;
            var loadedAssemblyDirectory = Path.GetDirectoryName(typeof(GameModuleBundle).Assembly.Location)!;
            Require(GameModuleBundle.ResolveRoot("") == loadedAssemblyDirectory,
                "empty host base directory did not resolve the loaded shared assembly directory");
            Require(GameModuleBundle.ResolveRoot(" ") == loadedAssemblyDirectory,
                "empty host base directory resolved against the current working directory");
            Require(Path.IsPathFullyQualified(GameModuleBundle.ResolveHostDirectory("")),
                "native library fallback directory is relative");
            Require(GameModuleBundle.ResolveRoot(root) == root, "explicit host directory changed");
            Environment.SetEnvironmentVariable(GameModuleBundle.RootEnvironmentVariable, root);
            Require(GameModuleBundle.ResolveRoot("") == root, "selected module root did not override empty host directory");
        }
        finally
        {
            Environment.CurrentDirectory = workingDirectory;
            Environment.SetEnvironmentVariable(GameModuleBundle.RootEnvironmentVariable, selected);
        }
        Console.WriteLine("host_bundle_root=passed empty_host_base=1 loaded_assembly=1 cwd_independent=1 explicit_root=1 selected_root=1");
    }
}
