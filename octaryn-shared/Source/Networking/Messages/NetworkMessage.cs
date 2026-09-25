namespace Octaryn.Shared.Networking;

public readonly record struct NetworkMessage(
    ReplicationId ReplicationId,
    uint MessageKind,
    ulong TickId);
