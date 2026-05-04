namespace Octaryn.Shared.GameModules;

public interface IModuleCommandRequests
{
    bool TryRequest(ModuleCommandRequest request);
}
