namespace Octaryn.Shared.GameModules;

public static partial class GameModuleValidator
{
    private static void RequireText(ModuleValidationReport report, string value, string code, string message)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            report.AddError(code, message);
        }
    }

    private static void RequireUnique(
        ModuleValidationReport report,
        IEnumerable<string> values,
        string code,
        string message)
    {
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var value in values)
        {
            if (string.IsNullOrWhiteSpace(value))
            {
                report.AddError(code, message);
                continue;
            }

            if (!seen.Add(value))
            {
                report.AddError(code, $"{message} Value: {value}");
            }
        }
    }

    private static void RequireVersion(ModuleValidationReport report, string value, string code, string message)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return;
        }

        if (!Version.TryParse(value, out _))
        {
            report.AddError(code, $"{message} Value: {value}");
        }
    }

    private static void RequireVersionRange(
        ModuleValidationReport report,
        string minimumVersion,
        string maximumVersion,
        string code,
        string message)
    {
        if (!Version.TryParse(minimumVersion, out var minimum) ||
            !Version.TryParse(maximumVersion, out var maximum))
        {
            return;
        }

        if (minimum > maximum)
        {
            report.AddError(code, $"{message} Minimum: {minimumVersion} Maximum: {maximumVersion}");
        }
    }

    private static void RequireVocabulary(
        ModuleValidationReport report,
        string value,
        IReadOnlySet<string> vocabulary,
        string code,
        string message)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return;
        }

        if (!vocabulary.Contains(value))
        {
            report.AddError(code, $"{message} Value: {value}");
        }
    }

    private static void RequireSafeRelativePath(ModuleValidationReport report, string path, string code, string message)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        if (path.StartsWith('/'.ToString(), StringComparison.Ordinal) ||
            path.StartsWith('\\'.ToString(), StringComparison.Ordinal) ||
            path.EndsWith('/'.ToString(), StringComparison.Ordinal) ||
            path.Contains('\\', StringComparison.Ordinal) ||
            path.Contains(':', StringComparison.Ordinal) ||
            path.Split('/').Any(segment => string.IsNullOrWhiteSpace(segment) ||
                segment == "." ||
                segment == ".."))
        {
            report.AddError(code, $"{message} Value: {path}");
        }
    }

    private static void RequireAllowed(
        ModuleValidationReport report,
        IReadOnlyList<string> values,
        Func<string, bool> isAllowed,
        string code,
        string message)
    {
        foreach (var value in values)
        {
            if (string.IsNullOrWhiteSpace(value))
            {
                continue;
            }

            if (!isAllowed(value))
            {
                report.AddError(code, $"{message} Value: {value}");
            }
        }
    }

    private static void RequireDenied(
        ModuleValidationReport report,
        IReadOnlyList<string> values,
        IReadOnlyList<string> deniedValues,
        string code,
        string message)
    {
        var denied = new HashSet<string>(deniedValues, StringComparer.Ordinal);
        foreach (var value in values)
        {
            if (string.IsNullOrWhiteSpace(value))
            {
                continue;
            }

            if (denied.Contains(value))
            {
                report.AddError(code, $"{message} Value: {value}");
            }
        }
    }
}
