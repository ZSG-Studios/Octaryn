namespace Octaryn.Shared.GameModules;

public interface IGameModuleRegistration
{
    GameModuleManifest Manifest { get; }

    IGameModuleInstance CreateInstance(ModuleHostContext context);
}
