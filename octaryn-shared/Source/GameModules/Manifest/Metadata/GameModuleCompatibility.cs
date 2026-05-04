namespace Octaryn.Shared.GameModules;

public sealed record GameModuleCompatibility(
    string MinimumHostApiVersion,
    string MaximumHostApiVersion,
    string SaveCompatibilityId,
    bool SupportsMultiplayer);
