using Octaryn.Server.Simulation.Players;

namespace Octaryn.Server.Session;

// Produced only after authority consumed commands. The network thread owns
// both publication and LES, so no disk serialization or cross-thread wait.
internal readonly record struct SessionPlayerState(
    ulong Tick, double Seconds, ulong AcknowledgedInputFrame, PlayerState Player,
    float WorldDayFraction, double WorldTotalSeconds);
