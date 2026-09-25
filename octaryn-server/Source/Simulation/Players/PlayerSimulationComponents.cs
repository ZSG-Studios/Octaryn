using Octaryn.Shared.Host;

namespace Octaryn.Server.Simulation.Players;

internal readonly record struct PlayerSimulationIdentity(int Id, long Generation);

internal sealed partial class PlayerSimulationWorld
{
    private struct IdentityComponent { public PlayerSimulationIdentity Value; }
    private struct StateComponent { public PlayerState Value; }
    private struct CommandComponent
    {
        public HostFrameContext Frame;
        public bool Pending;
        public NativeTickResult Result;
    }
    private sealed class BodyComponent
    {
        public readonly IntPtr Handle;
        public BodyComponent(IntPtr handle) => Handle = handle;
    }
}
