namespace Octaryn.Shared.Networking;

public readonly record struct ServerSnapshot(
    ulong TickId,
    IReadOnlyList<ReplicationId> ReplicationIds);
