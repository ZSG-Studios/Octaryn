using Octaryn.Basegame.Gameplay.Actions;
using Octaryn.Basegame.Gameplay.Inventory;
using Octaryn.Basegame.Gameplay.Items;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Basegame.Gameplay.Time;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame;

// Module root: wires every gameplay system to the host APIs granted by the
// manifest. The engine owns ECS storage, cadence, transport, persistence and
// presentation; the module owns components (plain structs) and rules.
public sealed partial class GameContext : IGameModuleInstance, IGameModulePlayerAuthority, IGameModuleSaveState
{
    private readonly IHostEcsApi? _ecs;
    private readonly ModuleEntity _playerEntity;
    private readonly PlayerStepSystem _playerStep;
    private readonly WorldTimeSystem _time;
    private readonly InventorySystem _inventory;
    private readonly InteractionSystem _interaction;
    private readonly ItemSystem _items;
    private readonly ActionRouter _actions;

    private GameContext(ModuleHostContext host)
    {
        _ecs = host.Ecs;
        if (_ecs is not null)
        {
            _playerEntity = PlayerEntity.Create(_ecs);
        }

        _playerStep = new PlayerStepSystem(host.Player, host.Input, host.Physics, host.Diagnostics);
        _time = new WorldTimeSystem(host.Diagnostics);
        _inventory = new InventorySystem(host.Diagnostics);
        _items = new ItemSystem(host.Physics, host.Replication, host.Diagnostics, _inventory);
        _interaction = new InteractionSystem(host.Physics, host.Audio, host.Replication, host.Diagnostics, _items);
        // Starter loadout: gives the drop/pickup loop something to conserve.
        if (_ecs is not null)
        {
            _inventory.TryAdd(_ecs, ItemCatalog.Apple, 8, 16);
            _inventory.TryAdd(_ecs, ItemCatalog.Torch, 4, 16);
            _inventory.TryAdd(_ecs, ItemCatalog.Coin, 25, 99);
        }
        _actions = new ActionRouter(host.Ui);
        _actions.Register(InventorySystem.SelectActionPrefix, id => _ecs is not null && _inventory.HandleAction(_ecs, id));
        _actions.Register(InteractionSystem.UseActionPrefix, id => _ecs is not null && _interaction.HandleAction(_ecs, id));
        _actions.Register(ItemSystem.DropAction, id => _ecs is not null && _items.HandleAction(_ecs, id));
        _actions.Register(ItemSystem.DropStackAction, id => _ecs is not null && _items.HandleAction(_ecs, id));
        host.Diagnostics?.Write(
            HostLogLevel.Info,
            "octaryn.basegame activated " +
            $"ecs_api={(_ecs is not null ? 1 : 0)} " +
            $"player_authority={(_playerStep.Authoritative ? 1 : 0)} " +
            $"world_api={(host.World is not null ? 1 : 0)} " +
            $"ui_api={(host.Ui is not null ? 1 : 0)} " +
            $"audio_api={(host.Audio is not null ? 1 : 0)} " +
            $"replication_api={(host.Replication is not null ? 1 : 0)}");
    }

    public static GameContext Create(ModuleHostContext host)
    {
        return new GameContext(host);
    }

    // World-tick cadence: every system except the authoritative player step.
    public void Tick(in ModuleFrameContext frame)
    {
        if (_ecs is null)
        {
            return;
        }

        _time.Tick(_ecs, in frame);
        _actions.Tick();
        _interaction.Tick(_ecs);
        _items.Tick(_ecs, frame.DeltaSeconds);
    }

    // Player command cadence: the authoritative module player step.
    public void TickPlayer(in ModuleFrameContext frame)
    {
        if (_ecs is null)
        {
            return;
        }

        _playerStep.TickPlayer(_ecs, frame.DeltaSeconds);
    }

    public void Dispose()
    {
        if (_ecs is not null && _playerEntity.IsValid)
        {
            _ecs.DestroyEntity(_playerEntity);
        }
    }
}
