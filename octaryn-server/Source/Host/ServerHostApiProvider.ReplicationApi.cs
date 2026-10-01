using Octaryn.Shared.Host.Api;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class ReplicationApi : IHostReplicationApi
    {
        private readonly ServerHostApiProvider _owner;
        public int AvailableChangeCapacity => _owner._replicationChannel?.AvailableChangeCapacity ?? int.MaxValue;
        public int AvailableWorldItemCapacity => _owner.WorldItems.Available;
        public bool PublishWorldItem(in HostWorldItemPose pose) => _owner.WorldItems.Publish(pose, _owner._tickId());

        public ReplicationApi(ServerHostApiProvider owner)
        {
            _owner = owner;
        }

        public bool PublishChange(uint changeKind, ulong replicationId, ulong payload0, ulong payload1)
        {
            var data = new ModuleEventData
            {
                EventId = replicationId,
                Payload0 = changeKind,
                Payload1 = payload0,
                Payload2 = payload1
            };
            return _owner._replicationChannel?.Broadcast(in data) ?? false;
        }

        // The LES session channel carries fixed 32-byte ModuleEventData only;
        // variable byte payloads need the native mailbox table.
        public int SendMessage(ulong replicationId, ReadOnlySpan<byte> payload)
        {
            _ = replicationId;
            _ = payload;
            return -2;
        }
    }
}
