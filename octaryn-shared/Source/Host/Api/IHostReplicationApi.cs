namespace Octaryn.Shared.Host.Api;

public interface IHostReplicationApi
{
    // Capacity for synchronous changes on the serialized authority thread.
    int AvailableChangeCapacity { get; }
    int AvailableWorldItemCapacity { get; }
    bool PublishWorldItem(in HostWorldItemPose pose);

    bool PublishChange(uint changeKind, ulong replicationId, ulong payload0, ulong payload1);

    // Returns bytes queued, or a negative host error.
    int SendMessage(ulong replicationId, ReadOnlySpan<byte> payload);
}
