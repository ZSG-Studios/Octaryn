namespace Octaryn.Shared.GameModules;

public sealed record GameModuleDependency(
    string ModuleId,
    string MinimumVersion,
    string MaximumVersion,
    bool IsRequired);
