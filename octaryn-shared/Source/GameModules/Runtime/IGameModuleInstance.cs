namespace Octaryn.Shared.GameModules;

public interface IGameModuleInstance : IDisposable
{
    void Tick(in ModuleFrameContext frame);
}
