using Movement = Octaryn.Basegame.Gameplay.MovementRules.MovementRules;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Player;

// Authoritative player step: polls the host input, advances the body through
// the host physics world and publishes the new state back to the host. All
// state lives in PlayerBodyComponent inside the host ECS; the system holds
// only API handles.
public sealed class PlayerStepSystem
{
    private readonly IHostPlayerApi? _player;
    private readonly IHostInputApi? _input;
    private readonly IHostPhysicsApi? _physics;
    private readonly IHostDiagnosticsApi? _diagnostics;

    public PlayerStepSystem(
        IHostPlayerApi? player,
        IHostInputApi? input,
        IHostPhysicsApi? physics,
        IHostDiagnosticsApi? diagnostics)
    {
        _player = player;
        _input = input;
        _physics = physics;
        _diagnostics = diagnostics;
    }

    public bool Authoritative => _player is not null && _physics is not null;

    public void TickPlayer(IHostEcsApi ecs, double deltaSeconds)
    {
        if (_player is null || _physics is null)
        {
            return;
        }

        ecs.Query<PlayerBodyComponent>((ModuleEntity _, ref PlayerBodyComponent body) =>
        {
            if (!body.Initialized)
            {
                if (!_player.TryGetState(out var spawned))
                {
                    return;
                }

                body.State = spawned;
                body.Initialized = true;
                _diagnostics?.Write(
                    HostLogLevel.Info,
                    $"octaryn.basegame player_authority active=1 spawn=({spawned.X:F3},{spawned.Y:F3},{spawned.Z:F3})");
            }

            if (_input is null || !_input.TryPollInput(out var input))
            {
                input = new HostInputState(
                    body.State.JumpHeld ? Movement.JumpFlag : 0u,
                    1u,
                    0.0f,
                    0.0f,
                    0.0f,
                    body.Pitch,
                    body.Yaw);
            }

            var command = Movement.ToCharacterInput(in input);
            var state = body.State;
            if (!_physics.MoveCharacter(in command, deltaSeconds, ref state))
            {
                if (!body.StepFailed)
                {
                    body.StepFailed = true;
                    _diagnostics?.Write(
                        HostLogLevel.Warning,
                        "octaryn.basegame player_authority step_failed=1");
                }

                return;
            }

            body.StepFailed = false;
            body.State = state;
            _player.SetState(in state);
        });
    }
}
