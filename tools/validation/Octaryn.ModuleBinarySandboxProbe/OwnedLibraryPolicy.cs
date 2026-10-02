using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

// Exact source projects are supplied by the engine invocation, never a module manifest.
internal static class OwnedLibraryPolicy
{
    public static bool IsVerifiedProject(JsonElement root,string targetKey,IReadOnlyList<string>? projects)
    {
        if(projects is null || !root.TryGetProperty("libraries",out var libraries) ||
           !libraries.TryGetProperty(targetKey,out var library) ||
           !library.TryGetProperty("type",out var type) || type.GetString()!="project" ||
           !library.TryGetProperty("path",out var path) || path.GetString() is not { } relative ||
           !root.TryGetProperty("project",out var project) || !project.TryGetProperty("restore",out var restore) ||
           !restore.TryGetProperty("projectPath",out var module) || module.GetString() is not { } modulePath)return false;
        var directory=Path.GetDirectoryName(Path.GetFullPath(modulePath));if(directory is null)return false;
        var referenced=Path.GetFullPath(relative.Replace('\\',Path.DirectorySeparatorChar),directory);
        var slash=targetKey.LastIndexOf('/');var name=slash<0?targetKey:targetKey[..slash];
        foreach(var expected in projects)
            if(File.Exists(expected) && Path.GetFileNameWithoutExtension(expected)==name &&
               string.Equals(referenced,Path.GetFullPath(expected),OperatingSystem.IsWindows()?StringComparison.OrdinalIgnoreCase:StringComparison.Ordinal))return true;
        return false;
    }
}
