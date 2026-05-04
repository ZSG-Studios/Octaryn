using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.Players;

internal readonly record struct PlayerSaveState(
    float X,
    float Y,
    float Z,
    float Pitch,
    float Yaw,
    BlockId SelectedBlock);
