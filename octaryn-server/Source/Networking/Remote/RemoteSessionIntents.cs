using System.Text;
using Arch.Core;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

internal sealed partial class RemoteSession
{
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
        if (kind is (byte)RemoteIntentKind.ChunkView
            or (byte)RemoteIntentKind.BlockInteraction or (byte)RemoteIntentKind.WorldTime
            or (byte)RemoteIntentKind.WorldItems or (byte)RemoteIntentKind.BlockResultsAck)
        {
            _world.Query(in _sessionQuery,
                (ref SessionConnectionComponent _, ref SessionIntentComponent intents, ref SessionPublishComponent _) =>
                intents.PendingIntents[(RemoteIntentKind)kind] = payload);
        }
    }

    private void FlushIntents()
    {
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent _, ref SessionIntentComponent intents, ref SessionPublishComponent publish) =>
            {
                foreach (var (kind, payload) in intents.PendingIntents.ToArray())
                {
                    var path = kind switch
                    {
                        RemoteIntentKind.ChunkView => _paths.ChunkViewIntent,
                        RemoteIntentKind.BlockInteraction => _paths.BlockInteractionIntent,
                        RemoteIntentKind.WorldTime => _paths.WorldTimeIntent,
                        RemoteIntentKind.WorldItems => _worldItemsIntentPath,
                        RemoteIntentKind.BlockResultsAck => Path.Combine(_runtimeDirectory, "block_results_ack.json"),
                        _ => null,
                    };
                    if (path is null || !WriteBytesAtomic(path, payload)) continue;
                    if (kind == RemoteIntentKind.BlockInteraction &&
                        TryReadFrameIndex(Encoding.UTF8.GetString(payload), out var frameIndex))
                        publish.PendingBlockAck = frameIndex;
                    intents.PendingIntents.Remove(kind);
                }
            });
    }
}
