using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

// Three bounded latest-intent slots; UI payloads contain the full unacknowledged
// journal, so coalescing retains ordering and all unacknowledged actions.
internal sealed partial class RemoteTransportClient
{
    private readonly byte[]?[] _intentSlots = new byte[7][];
    private readonly byte[]?[] _sentIntents = new byte[7][];
    private sealed record ActionAcknowledgement(ulong Epoch, ulong Sequence);
    private ActionAcknowledgement? _actionAcknowledgement;

    public bool SubmitIntent(byte kind, byte[] payload)
    {
        if (!IsRunning || kind is not (1 or 4 or 5) || payload.Length > 65536) return false;
        Volatile.Write(ref _intentSlots[kind], payload);
        return true;
    }

    public bool TryPollActionAcknowledgement(out ulong epoch, out ulong sequence)
    {
        var ack = Volatile.Read(ref _actionAcknowledgement);
        epoch = ack?.Epoch ?? 0;
        sequence = ack?.Sequence ?? 0;
        return ack is not null;
    }

    private void SyncIntents()
    {
        var controller = _controller;
        if (controller is null) return;
        for (byte kind = 0; kind < _intentSlots.Length; ++kind)
        {
            var payload = Volatile.Read(ref _intentSlots[kind]);
            if (payload is null || ReferenceEquals(payload, _sentIntents[kind])) continue;
            controller.SendIntent(kind, payload);
            if (kind == 1) Console.Error.WriteLine($"remote_session_window submitted=1 bytes={payload.Length}");
            _sentIntents[kind] = payload;
        }
        SendPlayerCommands();
    }

    private void PublishPose()
    {
        var entity = _entity;
        if (entity is null || !entity.TryReadPose(out var pose) || pose.SourceTick == _publishedPoseTick) return;
        PublishTypedPose(pose);
        _publishedPoseTick = pose.SourceTick;
        TracePose(in pose);
    }
}
