using Arch.Core;
using System.Text.Json;
using Octaryn.Server.Session;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

internal sealed partial class RemoteSession
{
    private NativeChunkViewIntent? _chunkView;
    internal void ReceivePlayerCommands(ReadOnlySpan<byte> packet)
    {
        var welcomed = false;
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent connection, ref SessionIntentComponent _, ref SessionPublishComponent _) =>
                welcomed = connection.Welcomed);
        if (welcomed) ChunkStreamProcessBridge.AcceptRemotePlayerCommands(PlayerCommandPacket.Decode(packet));
    }

    private void OnIntent(byte kind, byte[] payload)
    {
        if (payload.Length > RemoteProtocol.MaxIntentTextBytes) return;
        var welcomed = false;
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent connection, ref SessionIntentComponent _, ref SessionPublishComponent _) =>
                welcomed = connection.Welcomed);
        if (!welcomed) return;
        if (kind == (byte)RemoteIntentKind.EventAck) { AcknowledgeEvents(payload); return; }
        if (kind is (byte)RemoteIntentKind.ChunkView or (byte)RemoteIntentKind.WorldTime
            or (byte)RemoteIntentKind.UiAction)
        {
            _world.Query(in _sessionQuery,
                (ref SessionConnectionComponent _, ref SessionIntentComponent intents, ref SessionPublishComponent _) =>
                intents.PendingIntents[(RemoteIntentKind)kind] = payload);
        }
    }

    private void FlushIntents()
    {
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent _, ref SessionIntentComponent intents, ref SessionPublishComponent _) =>
            {
                foreach (var (kind, payload) in intents.PendingIntents.ToArray())
                {
                    if (kind == RemoteIntentKind.UiAction)
                    {
                        if (ChunkStreamProcessBridge.AcceptUiActionIntent(_gameModule, payload))
                            intents.PendingIntents.Remove(kind);
                        continue;
                    }
                    ApplyStateIntent(kind, payload);
                    intents.PendingIntents.Remove(kind);
                }
            });
    }
    private void ApplyStateIntent(RemoteIntentKind kind, byte[] payload)
    {
        try
        {
            using var json = JsonDocument.Parse(payload);
            var root = json.RootElement;
            if (root.GetProperty("version").GetInt32() != 1) return;
            if (kind == RemoteIntentKind.ChunkView)
            {
                var epoch = root.GetProperty("epoch").GetUInt64();
                var radius = root.GetProperty("radius").GetUInt32();
                if (radius is < 1 or > 32 || epoch == 0 || (_chunkView is { } old && epoch < old.Epoch)) return;
                _chunkView = new NativeChunkViewIntent(1, epoch,
                    root.GetProperty("centerChunkX").GetInt32(), root.GetProperty("centerChunkZ").GetInt32(),
                    radius, 0, 0, 0, 0);
                Console.Error.WriteLine($"server_session_window accepted=1 epoch={epoch} radius={radius}");
            }
            else if (kind == RemoteIntentKind.WorldTime)
            {
                var hours = root.GetProperty("hourOffset").GetInt32();
                if (hours is >= -1000000 and <= 1000000) _gameModule.SetWorldTimeHourOffset(hours);
            }
        }
        catch (Exception error) when (error is JsonException or InvalidOperationException or KeyNotFoundException or FormatException or OverflowException) { }
    }

}
