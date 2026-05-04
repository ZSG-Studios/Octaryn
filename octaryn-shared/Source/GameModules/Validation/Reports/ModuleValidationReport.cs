namespace Octaryn.Shared.GameModules;

public sealed class ModuleValidationReport
{
    private readonly List<ModuleValidationIssue> _issues = [];

    public IReadOnlyList<ModuleValidationIssue> Issues => _issues;

    public bool IsValid => !_issues.Any(issue => issue.Severity == ModuleValidationSeverity.Error);

    public void AddError(string code, string message)
    {
        _issues.Add(new ModuleValidationIssue(ModuleValidationSeverity.Error, code, message));
    }

    public void AddWarning(string code, string message)
    {
        _issues.Add(new ModuleValidationIssue(ModuleValidationSeverity.Warning, code, message));
    }
}
