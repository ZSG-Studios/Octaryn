namespace Octaryn.Shared.GameModules;

public readonly record struct ModuleValidationIssue(
    ModuleValidationSeverity Severity,
    string Code,
    string Message);
