using System.Buffers.Binary;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

internal sealed partial class RemoteSession
{
    private readonly Queue<SessionEventEnvelope> _eventJournal = new(256);
    private ulong _eventSequence;
    public int AvailableChangeCapacity => 256 - _eventJournal.Count;

    public bool Broadcast(in ModuleEventData data)
    {
        if (_sessionHigh == 0 && _sessionLow == 0) return false;
        if (AvailableChangeCapacity == 0) return false;
        var entry = new SessionEventEnvelope { Sequence = ++_eventSequence, Data = data };
        _eventJournal.Enqueue(entry);
        if (_peerWelcomed) _entity?.BroadcastModuleEvent(entry);
        return true;
    }

    private void AcknowledgeEvents(byte[] payload)
    {
        if (payload.Length != 8) return;
        var sequence = BinaryPrimitives.ReadUInt64LittleEndian(payload);
        if (sequence > _eventSequence) return;
        while (_eventJournal.TryPeek(out var entry) && entry.Sequence <= sequence) _eventJournal.Dequeue();
    }

    private void ReplayEvents()
    {
        foreach (var entry in _eventJournal) _entity?.BroadcastModuleEvent(entry);
    }

    private void ResetEventJournal()
    {
        _eventJournal.Clear();
        _eventSequence = 0;
    }
}
