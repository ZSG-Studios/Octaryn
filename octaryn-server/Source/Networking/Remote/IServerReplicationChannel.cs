using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

// Server-side broadcast channel for module-originated replication events.
// Implemented by RemoteSession once a dedicated session exists.
internal interface IServerReplicationChannel
{
    int AvailableChangeCapacity { get; }
    bool Broadcast(in ModuleEventData data);
}
