using Octaryn.Shared.World;

namespace Octaryn.Server.Simulation.Players;

internal readonly record struct PlayerState(
    float X,
    float Y,
    float Z,
    float Pitch,
    float Yaw,
    float VelocityX,
    float VelocityY,
    float VelocityZ,
    bool IsOnGround,
    PlayerControlMode ControlMode,
    BlockId SelectedBlock);
