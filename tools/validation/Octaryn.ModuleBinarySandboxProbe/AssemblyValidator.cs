using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using Octaryn.Shared.FrameworkAllowlist;

internal static class AssemblyValidator
{
    public static List<string> Validate(string assemblyPath, string? assetsPath, string? policyPath)
    {
        var errors = new List<string>();
        if (!File.Exists(assemblyPath))
        {
            errors.Add($"{assemblyPath}: assembly does not exist");
            return errors;
        }

        var allowedAssemblies = assetsPath is null
            ? null
            : AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assetsPath, policyPath, errors);

        using var stream = File.OpenRead(assemblyPath);
        using var peReader = new PEReader(stream);
        if (!peReader.HasMetadata)
        {
            errors.Add($"{assemblyPath}: assembly has no .NET metadata");
            return errors;
        }

        var metadata = peReader.GetMetadataReader();
        ValidateAssemblyReferences(errors, assemblyPath, metadata, allowedAssemblies);
        ValidateTypeReferences(errors, assemblyPath, metadata);
        ValidateMemberReferences(errors, assemblyPath, metadata);
        ValidatePinvokeMaps(errors, assemblyPath, metadata);
        return errors;
    }

    private static void ValidateAssemblyReferences(
        List<string> errors,
        string assemblyPath,
        MetadataReader metadata,
        IReadOnlySet<string>? allowedAssemblies)
    {
        foreach (var handle in metadata.AssemblyReferences)
        {
            var reference = metadata.GetAssemblyReference(handle);
            var name = metadata.GetString(reference.Name);
            if (name is "System.IO" or "System.Net.Http" or "System.Diagnostics.Process" or "System.Reflection.Emit")
            {
                errors.Add($"{assemblyPath}: denied assembly reference {name}");
                continue;
            }

            if (FrameworkAssemblies.IsFrameworkAssembly(name) &&
                !FrameworkAssemblies.IsTrustedFrameworkAssembly(metadata, reference))
            {
                errors.Add($"{assemblyPath}: untrusted framework assembly reference {name}");
                continue;
            }

            if (FrameworkAssemblies.IsFrameworkAssembly(name) &&
                HasLocalAssemblyFile(assemblyPath, name))
            {
                errors.Add($"{assemblyPath}: local framework assembly spoof is not allowed: {name}.dll");
                continue;
            }

            if (allowedAssemblies is not null &&
                !FrameworkAssemblies.IsFrameworkAssembly(name) &&
                !allowedAssemblies.Contains(name))
            {
                errors.Add($"{assemblyPath}: unexpected assembly reference {name}");
            }
        }
    }

    private static bool HasLocalAssemblyFile(string assemblyPath, string assemblyName)
    {
        var assemblyDirectory = Path.GetDirectoryName(Path.GetFullPath(assemblyPath));
        return assemblyDirectory is not null &&
            File.Exists(Path.Combine(assemblyDirectory, $"{assemblyName}.dll"));
    }

    private static void ValidateTypeReferences(
        List<string> errors,
        string assemblyPath,
        MetadataReader metadata)
    {
        foreach (var handle in metadata.TypeReferences)
        {
            var type = metadata.GetTypeReference(handle);
            ApiClassifier.ValidateTypeName(
                errors,
                assemblyPath,
                metadata.GetString(type.Namespace),
                metadata.GetString(type.Name),
                null);
        }
    }

    private static void ValidateMemberReferences(
        List<string> errors,
        string assemblyPath,
        MetadataReader metadata)
    {
        foreach (var handle in metadata.MemberReferences)
        {
            var member = metadata.GetMemberReference(handle);
            if (member.Parent.Kind != HandleKind.TypeReference)
            {
                continue;
            }

            var type = metadata.GetTypeReference((TypeReferenceHandle)member.Parent);
            ApiClassifier.ValidateTypeName(
                errors,
                assemblyPath,
                metadata.GetString(type.Namespace),
                metadata.GetString(type.Name),
                metadata.GetString(member.Name));
        }
    }

    private static void ValidatePinvokeMaps(
        List<string> errors,
        string assemblyPath,
        MetadataReader metadata)
    {
        foreach (var handle in metadata.MethodDefinitions)
        {
            var method = metadata.GetMethodDefinition(handle);
            if ((method.Attributes & MethodAttributes.PinvokeImpl) == 0)
            {
                continue;
            }

            errors.Add($"{assemblyPath}: denied framework API group {FrameworkApiGroupIds.BclNativeInterop}: P/Invoke method metadata");
        }
    }
}
