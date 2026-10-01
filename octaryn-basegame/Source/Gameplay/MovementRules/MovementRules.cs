using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.MovementRules;

// Module-owned movement rules: maps the polled input state to the kinematic
// character command the host mover understands. Flag bits match the host
// mover contract (jump, sprint, fly).
public static class MovementRules
{
    public const uint JumpFlag = 1u << 0;
    public const uint SprintFlag = 1u << 1;
    public const uint FlyFlag = 1u << 2;

    public static HostCharacterInput ToCharacterInput(in HostInputState input)
    {
        var flags = (input.Jump ? JumpFlag : 0u) |
            (input.Sprint ? SprintFlag : 0u) |
            (input.FlyMode ? FlyFlag : 0u);
        return new HostCharacterInput(
            flags,
            input.Controller,
            input.MoveX,
            input.MoveY,
            input.MoveZ,
            0.0f,
            0.0f,
            0.0f,
            input.CameraPitch,
            input.CameraYaw,
            1);
    }
}
