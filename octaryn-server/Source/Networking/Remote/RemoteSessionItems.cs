using System.Buffers.Binary;
using System.Diagnostics;
using LiteNetLib;
using LiteEntitySystem.Transport;
using Octaryn.Shared.Host.Api;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

internal sealed partial class RemoteSession
{
    private readonly HostWorldItemPose[] _itemBatch = new HostWorldItemPose[512];
    private HostWorldItemPose[]? _itemBaseline;
    private int _baselineOffset;
    private ulong _itemEpoch, _itemSequence, _itemWaiting;
    private long _nextItemSend;
    private bool _itemBaselineStarted;

    private void RestartItems()
    {
        _itemBaseline = _gameModule.WorldItems?.BeginBaseline() ?? [];
        _baselineOffset = 0; _itemBaselineStarted = false;
        ++_itemEpoch; _itemSequence = _itemWaiting = 0; _nextItemSend = 0;
    }

    internal void ReceiveItemAck(ReadOnlySpan<byte> bytes)
    {
        if (!_peerWelcomed || bytes.Length != 20 || bytes[1] != 1 || bytes[2] != 0 || bytes[3] != 0) return;
        var epoch = BinaryPrimitives.ReadUInt64LittleEndian(bytes[4..]);
        var sequence = BinaryPrimitives.ReadUInt64LittleEndian(bytes[12..]);
        if (epoch == _itemEpoch && sequence == _itemWaiting) _itemWaiting = 0;
    }

    private void SendItems()
    {
        if (!_peerWelcomed || _player is null || _itemWaiting != 0 || Stopwatch.GetTimestamp() < _nextItemSend) return;
        var baseline = _itemBaseline;
        int count;
        if (baseline is not null)
        {
            count = Math.Min(_itemBatch.Length, baseline.Length - _baselineOffset);
            baseline.AsSpan(_baselineOffset, count).CopyTo(_itemBatch);
        }
        else count = _gameModule.WorldItems?.Drain(_itemBatch) ?? 0;
        if (count == 0 && baseline is null) return;
        var packets = Math.Max(1, (count + WorldItemPacket.MaximumPoses - 1) / WorldItemPacket.MaximumPoses);
        for (var packet = 0; packet < packets; ++packet)
        {
            var start = packet * WorldItemPacket.MaximumPoses;
            var length = Math.Min(WorldItemPacket.MaximumPoses, count - start);
            byte flags = 0;
            if (baseline is not null && !_itemBaselineStarted) { flags |= WorldItemPacket.BeginBaseline; _itemBaselineStarted = true; }
            if (packet == packets - 1)
            {
                flags |= WorldItemPacket.EndBatch;
                if (baseline is not null && _baselineOffset + count == baseline.Length) flags |= WorldItemPacket.EndBaseline;
            }
            var bytes = WorldItemPacket.Encode(_itemEpoch, ++_itemSequence, flags, _itemBatch.AsSpan(start, length));
            _player.GetLiteNetLibNetPeer().NetPeer.Send(bytes, DeliveryMethod.ReliableOrdered);
        }
        _itemWaiting = _itemSequence;
        _nextItemSend = Stopwatch.GetTimestamp() + Stopwatch.Frequency / 20;
        if (baseline is not null)
        {
            _baselineOffset += count;
            if (_baselineOffset == baseline.Length) _itemBaseline = null;
        }
    }
}
