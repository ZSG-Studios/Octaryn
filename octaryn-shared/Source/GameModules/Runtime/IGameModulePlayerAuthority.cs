namespace Octaryn.Shared.GameModules;

// Optional module entry point for authoritative player steps. The host calls
// this once per consumed player command at the fixed player cadence when the
// module requests host.player; the regular Tick continues to run per world
// tick for all other systems.
public interface IGameModulePlayerAuthority
{
    void TickPlayer(in ModuleFrameContext frame);
}
