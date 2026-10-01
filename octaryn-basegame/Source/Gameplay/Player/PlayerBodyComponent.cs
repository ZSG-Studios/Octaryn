using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Player;

// Authoritative player body: the single source of truth for position,
// velocity and control state, stepped by PlayerStepSystem and replicated by
// the host. Other systems read pose from this component, never from APIs.
public struct PlayerBodyComponent
{
    public bool Initialized;
    public HostCharacterState State;
    public bool StepFailed;

    public readonly float EyeX => State.X;
    public readonly float EyeY => State.Y;
    public readonly float EyeZ => State.Z;
    public readonly float Pitch => State.Pitch;
    public readonly float Yaw => State.Yaw;
    public readonly bool IsFlying => State.ControlMode == 1u;
}
