using Movement = Octaryn.Basegame.Gameplay.MovementRules.MovementRules;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Player;

// Authoritative module player: consumes the host input each player step,
// advances the kinematic body through the host physics world and publishes
// the resulting state back to the host for replication and persistence.
public sealed class PlayerSystem
{
    private readonly IHostPlayerApi? _player;
    private readonly IHostInputApi? _input;
    private readonly IHostPhysicsApi? _physics;
    private readonly IHostDiagnosticsApi? _diagnostics;
    private readonly Action<HostCharacterState> _poseChanged;
    private HostCharacterState _state;
    private bool _initialized;
    private bool _stepFailed;

    public PlayerSystem(
        IHostPlayerApi? player,
        IHostInputApi? input,
        IHostPhysicsApi? physics,
        IHostDiagnosticsApi? diagnostics,
        Action<HostCharacterState> poseChanged)
    {
        _player = player;
        _input = input;
        _physics = physics;
        _diagnostics = diagnostics;
        _poseChanged = poseChanged;
    }

    public bool Authoritative => _player is not null && _physics is not null;

    public HostCharacterState State => _state;

    public void TickPlayer(double deltaSeconds)
    {
        if (_player is null || _physics is null)
        {
            return;
        }

        if (!_initialized)
        {
            if (!_player.TryGetState(out _state))
            {
                return;
            }

            _initialized = true;
            _diagnostics?.Write(
                HostLogLevel.Info,
                $"octaryn.basegame player_authority active=1 spawn=({_state.X:F3},{_state.Y:F3},{_state.Z:F3})");
        }

        if (_input is null || !_input.TryPollInput(out var input))
        {
            input = new HostInputState(
                _state.JumpHeld ? Movement.JumpFlag : 0u,
                1u,
                0.0f,
                0.0f,
                0.0f,
                _state.Pitch,
                _state.Yaw);
        }

        var command = Movement.ToCharacterInput(in input);
        if (!_physics.MoveCharacter(in command, deltaSeconds, ref _state))
        {
            if (!_stepFailed)
            {
                _stepFailed = true;
                _diagnostics?.Write(
                    HostLogLevel.Warning,
                    "octaryn.basegame player_authority step_failed=1");
            }

            return;
        }

        _stepFailed = false;
        _player.SetState(in _state);
        _poseChanged(_state);
    }
}
