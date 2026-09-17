using System.Text;
using System.Text.Json;
using Arch.Core;
using LiteEntitySystem;
using LiteEntitySystem.Transport;
using LiteNetLib;
using Octaryn.Server.Host;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Items;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

// One authoritative remote player session hosted on LiteEntitySystem. Network
// intents land in a private session directory and run through the same
// file-driven authority tick, publication and acknowledgement path as the
// local client session. Session state lives on a single Arch ECS entity; the
// LES SessionEntity replicates pose and carries snapshot/ack/intent RPCs.
// A single session drives the world, matching the local single-player
// authority model; extra connections are rejected until the active peer
// disconnects.
internal sealed class RemoteSession : IDisposable
{
    private const string ChunkViewFile = "chunk_view.json";
    private const string ChunkStreamFile = "chunk_stream.json";
    private const string PlayerInputFile = "player_input.json";
    private const string PlayerStateFile = "player_state.json";
    private const string BlockInteractionFile = "block_interaction.json";
    private const string WorldTimeFile = "world_time.json";
    private const string WorldItemsIntentFile = "world_items.intent";
    private const string WorldItemsSnapshotFile = "world_items.snapshot";

    private readonly ModuleActivator _gameModule;
    private readonly ServerEntityManager _entityManager;
    private readonly LiteNetManager _manager;
    private readonly SessionFilePaths _paths;
    private readonly Arch.Core.World _world = Arch.Core.World.Create();
    private readonly QueryDescription _sessionQuery = new QueryDescription()
        .WithAll<SessionConnectionComponent, SessionIntentComponent, SessionPublishComponent>();
    private readonly string _worldItemsIntentPath;
    private readonly string _worldItemsSnapshotPath;
    private readonly WorldItemsProcess _worldItems;
    private NetPlayer? _player;
    private SessionEntity? _entity;
    private SessionController? _controller;
    private Entity _archEntity;
    private byte[]? _lastItemSnapshotBytes;
    private bool _disposed;

    public RemoteSession(ModuleActivator gameModule, ServerEntityManager entityManager,
        LiteNetManager manager, string sessionDirectory)
    {
        _gameModule = gameModule;
        _entityManager = entityManager;
        _manager = manager;
        Directory.CreateDirectory(sessionDirectory);
        _paths = new SessionFilePaths(
            Path.Combine(sessionDirectory, ChunkViewFile),
            Path.Combine(sessionDirectory, ChunkStreamFile),
            Path.Combine(sessionDirectory, PlayerInputFile),
            Path.Combine(sessionDirectory, PlayerStateFile),
            Path.Combine(sessionDirectory, BlockInteractionFile),
            Path.Combine(sessionDirectory, WorldTimeFile),
            MetadataOnly: false);
        _worldItemsIntentPath = Path.Combine(sessionDirectory, WorldItemsIntentFile);
        _worldItemsSnapshotPath = Path.Combine(sessionDirectory, WorldItemsSnapshotFile);
        _worldItems = new WorldItemsProcess(gameModule, runtimeRoot: sessionDirectory);
    }

    public bool HasPeer => _player is not null;

    public bool OwnsPeer(LiteNetPeer peer) =>
        _player is not null && ReferenceEquals(_player.GetLiteNetLibNetPeer().NetPeer, peer);

    public bool TryAttach(NetPlayer player)
    {
        ArgumentNullException.ThrowIfNull(player);
        if (_player is not null)
        {
            return false;
        }

        _player = player;
        _archEntity = _world.Create(
            new SessionConnectionComponent { Welcomed = false, Label = string.Empty },
            new SessionIntentComponent(),
            new SessionPublishComponent());
        ClearSessionFiles();
        _lastItemSnapshotBytes = null;
        _worldItems.RequestSnapshot();
        _gameModule.ChunkPublication.Reset();
        ChunkStreamProcessBridge.ResetSessionState();
        _entity = _entityManager.AddEntity<SessionEntity>(entity => { });
        _controller = _entityManager.AddController<SessionController>(player, controller =>
        {
            controller.HelloReceived += OnHello;
            controller.IntentReceived += OnIntent;
        });
        LiveDebugLog.Write($"server_remote_peer attached=1 endpoint={player.Peer}");
        return true;
    }

    public void Detach(string reason)
    {
        if (_player is null)
        {
            return;
        }

        var endpoint = _player.Peer.ToString();
        var player = _player;
        if (_controller is not null)
        {
            _controller.HelloReceived -= OnHello;
            _controller.IntentReceived -= OnIntent;
            _controller = null;
        }

        // The server-owned session entity is not covered by RemovePlayer.
        if (_entity is not null)
        {
            _entity.Destroy();
            _entity = null;
        }

        _world.Destroy(_archEntity);
        _player = null;
        // RemovePlayer destroys the player-owned controller.
        _entityManager.RemovePlayer(player);
        LiveDebugLog.Write($"server_remote_peer attached=0 reason={reason} endpoint={endpoint}");
    }

    public void Step()
    {
        var player = _player;
        var entity = _entity;
        if (player is null || entity is null)
        {
            return;
        }

        var welcomed = false;
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent connection, ref SessionIntentComponent _, ref SessionPublishComponent _) =>
            welcomed = connection.Welcomed);
        if (!welcomed)
        {
            return;
        }

        var hasChunkView = false;
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent _, ref SessionIntentComponent intents, ref SessionPublishComponent _) =>
            hasChunkView = intents.PendingIntents.ContainsKey(RemoteIntentKind.ChunkView));
        if (!hasChunkView && !File.Exists(_paths.ChunkViewIntent))
        {
            return;
        }

        FlushIntents();
        if (ChunkStreamProcessBridge.HandleSessionPaths(_gameModule, _paths, allowMissingIntent: true) != 0)
        {
            LiveDebugLog.Write("server_remote_session_step failed=1");
            _manager.DisconnectPeer(player.GetLiteNetLibNetPeer().NetPeer);

            return;
        }

        try { _worldItems.Step(); }
        catch (Exception error) when (WorldItemsProcess.IsTransientFileContention(error))
        {
            return;
        }
        PublishPose(entity);
        PublishSnapshot(entity);
        PublishItemSnapshot(entity);
        AcknowledgeInteraction(entity);
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        Detach("dispose");
        try { _worldItems.Dispose(); }
        finally { _world.Dispose(); }
    }

    private void OnHello(ulong version)
    {
        if (version != RemoteProtocol.Version)
        {
            LiveDebugLog.Write($"server_remote_hello rejected=1 reason=protocol-mismatch protocol={version}");
            if (_player is not null)
            {
                _manager.DisconnectPeer(_player.GetLiteNetLibNetPeer().NetPeer);
            }

            return;
        }

        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent connection, ref SessionIntentComponent _, ref SessionPublishComponent _) =>
            connection.Welcomed = true);
        _entity?.SendWelcome(RemoteProtocol.Version);
        LiveDebugLog.Write($"server_remote_hello accepted=1 label=octaryn-client endpoint={_player?.Peer}");
    }

    private void OnIntent(byte kind, byte[] payload)
    {
        if (kind is (byte)RemoteIntentKind.ChunkView or (byte)RemoteIntentKind.PlayerInput
            or (byte)RemoteIntentKind.BlockInteraction or (byte)RemoteIntentKind.WorldTime
            or (byte)RemoteIntentKind.WorldItems)
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
                        RemoteIntentKind.PlayerInput => _paths.PlayerInputIntent,
                        RemoteIntentKind.BlockInteraction => _paths.BlockInteractionIntent,
                        RemoteIntentKind.WorldTime => _paths.WorldTimeIntent,
                        RemoteIntentKind.WorldItems => _worldItemsIntentPath,
                        _ => null,
                    };
                    if (path is null)
                    {
                        continue;
                    }

                    if (!WriteBytesAtomic(path, payload)) continue;

                    if (kind == RemoteIntentKind.BlockInteraction &&
                        TryReadFrameIndex(Encoding.UTF8.GetString(payload), out var frameIndex))
                    {
                        publish.PendingBlockAck = frameIndex;
                    }

                    intents.PendingIntents.Remove(kind);
                }

            });
    }

    private void PublishPose(SessionEntity entity)
    {
        var bytes = TryReadBytes(_paths.PlayerStateStream);
        if (bytes is null)
        {
            return;
        }

        var duplicate = false;
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent _, ref SessionIntentComponent _, ref SessionPublishComponent publish) =>
            {
                duplicate = ByteArraysEqual(bytes, publish.LastPoseBytes);
                if (!duplicate)
                {
                    publish.LastPoseBytes = bytes;
                }
            });
        if (duplicate)
        {
            return;
        }

        PlayerStatePayload pose;
        try
        {
            pose = JsonSerializer.Deserialize<PlayerStatePayload>(bytes);
        }
        catch (JsonException)
        {
            return;
        }

        entity.PublishPose(pose.frameIndex, pose.acknowledgedInputFrame, pose.sourceTick, pose.sourceSeconds,
            pose.playerX, pose.playerY, pose.playerZ, pose.playerPitch, pose.playerYaw,
            pose.playerVelocityX, pose.playerVelocityY, pose.playerVelocityZ,
            pose.playerOnGround != 0, pose.playerControlMode == 1,
            pose.worldTimeDayFraction, pose.worldTimeTotalSeconds);
    }

    private void PublishSnapshot(SessionEntity entity)
    {
        var bytes = TryReadBytes(_paths.ChunkStream + ".bin");
        if (bytes is null)
        {
            return;
        }

        var duplicate = false;
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent _, ref SessionIntentComponent _, ref SessionPublishComponent publish) =>
            {
                duplicate = ByteArraysEqual(bytes, publish.LastSnapshotBytes);
                if (!duplicate)
                {
                    publish.LastSnapshotBytes = bytes;
                }
            });
        if (duplicate)
        {
            return;
        }

        entity.SendSnapshot(bytes);
        LiveDebugLog.Write($"server_remote_chunk_snapshot sent=1 bytes={bytes.Length}");
    }

    private void PublishItemSnapshot(SessionEntity entity)
    {
        var bytes = TryReadBytes(_worldItemsSnapshotPath);
        if (bytes is null || ByteArraysEqual(bytes, _lastItemSnapshotBytes)) return;
        entity.SendItemSnapshot(bytes);
        _lastItemSnapshotBytes = bytes;
    }

    private void AcknowledgeInteraction(SessionEntity entity)
    {
        var pending = 0ul;
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent _, ref SessionIntentComponent _, ref SessionPublishComponent publish) =>
            pending = publish.PendingBlockAck);
        if (pending == 0 || File.Exists(_paths.BlockInteractionIntent))
        {
            return;
        }

        entity.SendBlockAck(pending);
        LiveDebugLog.Write($"server_remote_block_ack frame={pending}");
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent _, ref SessionIntentComponent _, ref SessionPublishComponent publish) =>
            publish.PendingBlockAck = 0);
    }

    private void ClearSessionFiles()
    {
        foreach (var path in new[]
        {
            _paths.ChunkViewIntent, _paths.ChunkStream, _paths.ChunkStream + ".bin",
            _paths.PlayerInputIntent, _paths.PlayerStateStream,
            _paths.BlockInteractionIntent, _paths.WorldTimeIntent,
            _worldItemsIntentPath, _worldItemsSnapshotPath,
        })
        {
            TryDelete(path);
        }
    }

    private static bool TryReadFrameIndex(string payload, out ulong frameIndex)
    {
        frameIndex = 0;
        try
        {
            using var document = JsonDocument.Parse(payload);
            if (document.RootElement.TryGetProperty("frameIndex", out var value) &&
                value.TryGetUInt64(out frameIndex) && frameIndex != 0)
            {
                return true;
            }
        }
        catch (JsonException)
        {
        }

        return false;
    }

    // Retain failed intents until a later tick can publish them atomically.
    private static bool WriteBytesAtomic(string path, byte[] payload)
    {
        var temporary = path + ".tmp";
        for (var attempt = 0; attempt < 20; attempt++)
        {
            try
            {
                File.WriteAllBytes(temporary, payload);
                File.Move(temporary, path, overwrite: true);
                return true;
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                Thread.Sleep(1);
            }
        }
        return false;
    }

    private static byte[]? TryReadBytes(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return null;
        }

        try
        {
            return File.ReadAllBytes(path);
        }
        catch (IOException)
        {
            return null;
        }
        catch (UnauthorizedAccessException)
        {
            return null;
        }
    }

    private static void TryDelete(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        try
        {
            File.Delete(path);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static bool ByteArraysEqual(byte[] left, byte[]? right)
    {
        return right is not null && left.AsSpan().SequenceEqual(right);
    }

    private struct PlayerStatePayload
    {
        public int version { get; set; }
        public string? source { get; set; }
        public ulong frameIndex { get; set; }
        public ulong acknowledgedInputFrame { get; set; }
        public ulong sourceTick { get; set; }
        public double sourceSeconds { get; set; }
        public float worldTimeDayFraction { get; set; }
        public double worldTimeTotalSeconds { get; set; }
        public float playerX { get; set; }
        public float playerY { get; set; }
        public float playerZ { get; set; }
        public float playerPitch { get; set; }
        public float playerYaw { get; set; }
        public float playerVelocityX { get; set; }
        public float playerVelocityY { get; set; }
        public float playerVelocityZ { get; set; }
        public uint playerControlMode { get; set; }
        public uint playerOnGround { get; set; }
    }
}
