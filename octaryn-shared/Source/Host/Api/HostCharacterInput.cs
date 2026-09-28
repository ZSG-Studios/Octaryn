namespace Octaryn.Shared.Host.Api;

// Kinematic character mover input; flag bits match HostInputState.
public readonly record struct HostCharacterInput(
    uint Flags,
    uint Controller,
    float MoveX,
    float MoveY,
    float MoveZ,
    float CameraX,
    float CameraY,
    float CameraZ,
    float CameraPitch,
    float CameraYaw,
    int RelativeMouse);
