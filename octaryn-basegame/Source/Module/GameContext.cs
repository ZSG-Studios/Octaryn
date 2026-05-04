using Arch.Core;
using Octaryn.Basegame.Gameplay.Rules;
using Octaryn.Shared.GameModules;

namespace Octaryn.Basegame;

public sealed class GameContext : IGameModuleInstance
{
    private readonly World _world = World.Create();
    private Entity _bootstrapEntity;
    private bool _isBootstrapEntityCreated;

    private GameContext(ModuleHostContext host)
    {
        _ = host;
    }

    public static GameContext Create(ModuleHostContext host)
    {
        return new GameContext(host);
    }

    public void Tick(in ModuleFrameContext frame)
    {
        EnsureBootstrapEntity();
        _world.Set(_bootstrapEntity, new FrameState(
            frame.DeltaSeconds,
            frame.FrameIndex));
    }

    public void Dispose()
    {
        _world.Dispose();
    }

    private void EnsureBootstrapEntity()
    {
        if (_isBootstrapEntityCreated)
        {
            return;
        }

        _bootstrapEntity = _world.Create(
            new EntityTag(),
            new FrameState(0.0, 0));
        _isBootstrapEntityCreated = true;
    }
}
