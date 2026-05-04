using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;
using Octaryn.Shared.GameModules;

namespace Octaryn.Client.Host;

internal static class BundledModuleLoader
{
    private static bool s_resolverAttached;

    public static IGameModuleRegistration LoadBundledRegistration()
    {
        AttachResolver();
        LoadAssembly("Octaryn.Shared");
        var moduleId = ResolveBundledModuleId();
        var assemblyName = ModuleAssemblyName(moduleId);
        var registrationType = ModuleRegistrationTypeName(assemblyName);
        var assembly = LoadAssembly(assemblyName);
        var type = assembly.GetType(registrationType, throwOnError: true)!;
        if (!typeof(IGameModuleRegistration).IsAssignableFrom(type))
        {
            throw new InvalidOperationException($"{registrationType} does not implement {nameof(IGameModuleRegistration)}.");
        }

        return Activator.CreateInstance(type) is IGameModuleRegistration registration
            ? registration
            : throw new InvalidOperationException($"{registrationType} could not be created.");
    }

    private static string ResolveBundledModuleId()
    {
        var moduleDirectory = Path.Combine(ModuleDirectory, "Data", "Module");
        var manifest = Directory.EnumerateFiles(moduleDirectory, "*.module.json").Order().FirstOrDefault()
            ?? throw new InvalidOperationException($"No bundled game module manifest was found in {moduleDirectory}.");
        using var stream = File.OpenRead(manifest);
        using var document = JsonDocument.Parse(stream);
        return document.RootElement.GetProperty("ModuleId").GetString()
            ?? throw new InvalidOperationException($"Bundled game module manifest {manifest} has no ModuleId.");
    }

    private static string ModuleAssemblyName(string moduleId)
    {
        return string.Join('.', moduleId.Split('.', StringSplitOptions.RemoveEmptyEntries).Select(ToPascalCase));
    }

    private static string ModuleRegistrationTypeName(string assemblyName)
    {
        return $"{assemblyName}.Module.ModuleRegistration";
    }

    private static string ToPascalCase(string value)
    {
        return string.Concat(value.Split(['-', '_'], StringSplitOptions.RemoveEmptyEntries)
            .Select(part => char.ToUpperInvariant(part[0]) + part[1..]));
    }

    private static Assembly LoadAssembly(string assemblyName)
    {
        var assemblyPath = Path.Combine(ModuleDirectory, $"{assemblyName}.dll");
        if (File.Exists(assemblyPath))
        {
            return ModuleLoadContext.LoadFromAssemblyPath(assemblyPath);
        }

        return Assembly.Load(assemblyName);
    }

    private static string ModuleDirectory =>
        Path.GetDirectoryName(typeof(BundledModuleLoader).Assembly.Location) ?? AppContext.BaseDirectory;

    private static AssemblyLoadContext ModuleLoadContext =>
        AssemblyLoadContext.GetLoadContext(typeof(BundledModuleLoader).Assembly) ?? AssemblyLoadContext.Default;

    private static void AttachResolver()
    {
        if (s_resolverAttached)
        {
            return;
        }

        ModuleLoadContext.Resolving += ResolveFromBundleDirectory;
        s_resolverAttached = true;
    }

    private static Assembly? ResolveFromBundleDirectory(AssemblyLoadContext context, AssemblyName name)
    {
        var assemblyPath = Path.Combine(ModuleDirectory, $"{name.Name}.dll");
        return File.Exists(assemblyPath) ? context.LoadFromAssemblyPath(assemblyPath) : null;
    }
}
