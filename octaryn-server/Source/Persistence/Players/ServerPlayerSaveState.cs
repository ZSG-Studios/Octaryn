using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.Players;

internal readonly record struct ServerPlayerSaveState(
    float X,
    float Y,
    float Z,
    float Pitch,
    float Yaw,
    BlockId SelectedBlock);
