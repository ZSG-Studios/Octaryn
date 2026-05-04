using System;
using System.Reflection.Metadata;

internal static class FrameworkAssemblies
{
    public static bool IsFrameworkAssembly(string assemblyName)
    {
        return assemblyName == "netstandard" ||
            assemblyName == "System" ||
            assemblyName.StartsWith("System.", StringComparison.Ordinal) ||
            assemblyName == "Microsoft.CSharp";
    }

    public static bool IsTrustedFrameworkAssembly(MetadataReader metadata, AssemblyReference reference)
    {
        var name = metadata.GetString(reference.Name);
        if (!IsFrameworkAssembly(name))
        {
            return false;
        }

        var token = metadata.GetBlobBytes(reference.PublicKeyOrToken);
        var tokenText = Convert.ToHexString(token).ToLowerInvariant();
        return tokenText is "b03f5f7f11d50a3a" or "cc7b13ffcd2ddd51";
    }
}
