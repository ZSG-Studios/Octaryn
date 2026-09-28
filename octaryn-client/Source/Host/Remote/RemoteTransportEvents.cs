using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

// Module event ingestion: the authority broadcasts fixed-size module events
// (grants, drop receipts, look-target highlights) over the session entity
// RPC. They queue here for the native frame loop to poll; overflow fails the
// session visibly instead of dropping authoritative inventory receipts.
internal sealed partial class RemoteTransportClient
{
    private const int MaximumPendingModuleEvents = 256;
    private readonly Queue<SessionEventEnvelope> _moduleEvents = new();
    private ulong _receivedEventSequence;

    public bool TryPollModuleEvent(out ModuleEventData data)
    {
        lock (_mutex)
        {
            while (_moduleEvents.TryDequeue(out var envelope))
            {
                var acknowledgement = new byte[8];
                System.Buffers.Binary.BinaryPrimitives.WriteUInt64LittleEndian(acknowledgement, envelope.Sequence);
                Volatile.Write(ref _intentSlots[6], acknowledgement);
                if (envelope.Data.Payload0 == RemoteProtocol.UiActionAckEventKind) continue;
                data = envelope.Data;
                return true;
            }
            data = default;
            return false;
        }
    }

    private void OnModuleEvent(SessionEventEnvelope envelope)
    {
        lock (_mutex)
        {
            if (envelope.Sequence <= _receivedEventSequence) return;
            if (envelope.Sequence != _receivedEventSequence + 1)
            {
                Fail("error: remote module event sequence gap");
                return;
            }
            var data = envelope.Data;
            if (_moduleEvents.Count >= MaximumPendingModuleEvents)
            {
                Fail("error: server exceeded the bounded unconsumed event journal");
                return;
            }
            if (data.Payload0 == RemoteProtocol.UiActionAckEventKind)
            {
                Volatile.Write(ref _actionAcknowledgement, new ActionAcknowledgement(data.Payload1, data.EventId));
            }
            _moduleEvents.Enqueue(envelope);
            _receivedEventSequence = envelope.Sequence;
        }
    }
}
