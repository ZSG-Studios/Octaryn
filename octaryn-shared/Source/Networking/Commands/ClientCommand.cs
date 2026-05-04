namespace Octaryn.Shared.Networking;

public readonly record struct ClientCommand(
    ReplicationId SourceId,
    uint CommandKind,
    ulong TickId);
