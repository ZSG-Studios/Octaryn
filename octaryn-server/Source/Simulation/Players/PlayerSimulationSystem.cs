using Arch.Core;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Simulation.Players;

internal sealed partial class PlayerSimulationWorld
{
    private readonly QueryDescription _query = new QueryDescription()
        .WithAll<IdentityComponent, StateComponent, CommandComponent, BodyComponent>();

    // At most one pending command per actor. The caller owns acceptance, sequence and time budgets.
    public void Queue(PlayerSimulationIdentity identity, in HostFrameContext frame)
    {
        ref var command = ref _world.Get<CommandComponent>(Find(identity));
        if (command.Pending) throw new InvalidOperationException("Player already has a queued command.");
        command.Frame = frame;
        command.Pending = true;
    }

    public void StepQueued() => Execute(null);

    public PlayerState StepOne(PlayerSimulationIdentity identity, in HostFrameContext frame, out NativeTickResult result)
    {
        Queue(identity, frame);
        Execute(identity);
        var entity = Find(identity);
        result = _world.Get<CommandComponent>(entity).Result;
        return _world.Get<StateComponent>(entity).Value;
    }

    private void Execute(PlayerSimulationIdentity? only)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var system = new StepSystem(_simulation, only, _mapWorld);
        _world.InlineQuery<StepSystem, IdentityComponent, StateComponent, CommandComponent, BodyComponent>(in _query, ref system);
    }

    private readonly struct StepSystem(NativePlayerSimulation simulation, PlayerSimulationIdentity? only, IntPtr? mapWorld)
        : IForEach<IdentityComponent, StateComponent, CommandComponent, BodyComponent>
    {
        public void Update(ref IdentityComponent identity, ref StateComponent state,
            ref CommandComponent command, ref BodyComponent body)
        {
            if (!command.Pending || (only.HasValue && only.Value != identity.Value)) return;
            // Consume first so a failed call cannot replay a partially applied native step.
            command.Pending = false;
            state.Value = mapWorld.HasValue
                ? simulation.StepWithMap(mapWorld.Value, body.Handle, command.Frame.Input,
                    command.Frame.DeltaSeconds, out command.Result)
                : simulation.Step(body.Handle, command.Frame.Input,
                    command.Frame.DeltaSeconds, out command.Result);
        }
    }
}
