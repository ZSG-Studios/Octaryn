using System.Diagnostics;
using LiteNetLib;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

internal sealed partial class RemoteTransportClient
{
    private long _lastCommandSend;

    private void SendPlayerCommands()
    {
        if (_peer is null || !_welcomed) return;
        var now = Stopwatch.GetTimestamp();
        if (_lastCommandSend != 0 && Stopwatch.GetElapsedTime(_lastCommandSend, now).TotalSeconds < 1.0 / 60) return;
        _lastCommandSend = now;
        byte[] bytes;
        try { bytes = File.ReadAllBytes(Path.Combine(_runtimeDirectory, PlayerInputFile)); }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException) { return; }
        var commands = PlayerCommandPacket.ReadJson(bytes);
        var capacity = Math.Min(PlayerCommandPacket.MaxDatagramCommands,
            (_peer.GetMaxSinglePacketSize(DeliveryMethod.Unreliable) - PlayerCommandPacket.HeaderSize) / PlayerCommandPacket.CommandSize);
        if (capacity <= 0) return;
        for (var offset = 0; offset < commands.Length; offset += capacity)
        {
            var packet = PlayerCommandPacket.Encode(commands.AsSpan(offset, Math.Min(capacity, commands.Length - offset)));
            _peer.Send(packet, DeliveryMethod.Unreliable);
        }
        TraceIntent(RemoteIntentKind.PlayerInput, bytes);
    }
}
