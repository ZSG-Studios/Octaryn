using Arch.Core;
using LiteEntitySystem;
using LiteEntitySystem.Transport;
using LiteNetLib;
using Octaryn.Server.Host;
using Octaryn.Server.Modules;
using Octaryn.Server.Session;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

// One authoritative LES player session. Typed command batches and bounded
// intent journals feed authority directly; completed poses and receipts return
// through LES. The current host admits one peer per world.
internal sealed partial class RemoteSession : IDisposable, IServerReplicationChannel
{
    private const string ChunkViewFile = "chunk_view.json";
    private const string PlayerInputFile = "player_input.json";
    private const string PlayerStateFile = "player_state.json";
    private const string WorldTimeFile = "world_time.json";
    private const string UiActionFile = "ui_action.json";

    private readonly ModuleActivator _gameModule;
    private readonly ServerEntityManager _entityManager;
    private readonly LiteNetManager _manager;
    private readonly SessionFilePaths _paths;
    private readonly Arch.Core.World _world = Arch.Core.World.Create();
    private readonly QueryDescription _sessionQuery = new QueryDescription()
        .WithAll<SessionConnectionComponent, SessionIntentComponent, SessionPublishComponent>();
    private readonly string _runtimeDirectory;
    private NetPlayer? _player;
    private SessionEntity? _entity;
    private SessionController? _controller;
    private Entity _archEntity;
    private bool _disposed;
    private ulong _sessionHigh, _sessionLow;
    private bool _peerWelcomed;
    private readonly Action<SessionPlayerState> _publishState;

    public RemoteSession(ModuleActivator gameModule, ServerEntityManager entityManager,
        LiteNetManager manager, string sessionDirectory)
    {
        _gameModule = gameModule;
        _publishState = PublishPose;
        _entityManager = entityManager;
        _manager = manager;
        Directory.CreateDirectory(sessionDirectory);
        _runtimeDirectory = sessionDirectory;
        _paths = new SessionFilePaths(
            Path.Combine(sessionDirectory, ChunkViewFile),
            Path.Combine(sessionDirectory, PlayerInputFile),
            Path.Combine(sessionDirectory, PlayerStateFile),
            Path.Combine(sessionDirectory, WorldTimeFile),
            Path.Combine(sessionDirectory, UiActionFile));
    }

    public bool HasPeer => _player is not null;

    // IServerReplicationChannel: module events ride the session entity RPC.
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
        _peerWelcomed = false;
        _chunkView = null;
        _archEntity = _world.Create(
            new SessionConnectionComponent { Welcomed = false, Label = string.Empty },
            new SessionIntentComponent(),
            new SessionPublishComponent());
        ClearSessionFiles();
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
        _peerWelcomed = false;
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

        FlushIntents();
        if (_chunkView is not { } chunkView) return;
        if (ChunkStreamProcessBridge.ExecuteSessionTick(_gameModule, chunkView, null, _publishState) != 0)
        {
            LiveDebugLog.Write("server_remote_session_step failed=1");
            _manager.DisconnectPeer(player.GetLiteNetLibNetPeer().NetPeer);

            return;
        }
        SendItems();
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        Detach("dispose");
        _world.Dispose();
    }

    private void OnHello(ulong version, ulong high, ulong low)
    {
        if (version != RemoteProtocol.Version || (high == 0 && low == 0) ||
            (_peerWelcomed && (high != _sessionHigh || low != _sessionLow)))
        {
            LiveDebugLog.Write($"server_remote_hello rejected=1 reason=protocol-mismatch protocol={version}");
            if (_player is not null)
            {
                _manager.DisconnectPeer(_player.GetLiteNetLibNetPeer().NetPeer);
            }

            return;
        }

        var resumed = high == _sessionHigh && low == _sessionLow;
        if (!resumed)
        {
            _gameModule.ResetUiActions();
            ChunkStreamProcessBridge.ResetSessionState();
            ResetEventJournal();
            (_sessionHigh, _sessionLow) = (high, low);
        }
        _world.Query(in _sessionQuery,
            (ref SessionConnectionComponent connection, ref SessionIntentComponent _, ref SessionPublishComponent _) =>
            connection.Welcomed = true);
        _peerWelcomed = true;
        _entity?.SendWelcome(new SessionWelcome { Version = RemoteProtocol.Version,
            SessionHigh = high, SessionLow = low, AcknowledgedInput = ChunkStreamProcessBridge.ConsumedPlayerCommand,
            Resumed = resumed ? 1UL : 0UL });
        ReplayEvents();
        RestartItems();
        LiveDebugLog.Write($"server_remote_resume resumed={(resumed ? 1 : 0)} ack={ChunkStreamProcessBridge.ConsumedPlayerCommand}");
        LiveDebugLog.Write($"server_remote_hello accepted=1 label=octaryn-client endpoint={_player?.Peer}");
    }

    private void PublishPose(SessionPlayerState snapshot)
    {
        var pose = snapshot.Player;
        _entity?.PublishPose(snapshot.Tick, snapshot.AcknowledgedInputFrame, snapshot.Tick, snapshot.Seconds,
            pose.X, pose.Y, pose.Z, pose.Pitch, pose.Yaw,
            pose.VelocityX, pose.VelocityY, pose.VelocityZ,
            pose.IsOnGround, pose.ControlMode == 1,
            snapshot.WorldDayFraction, snapshot.WorldTotalSeconds, pose.JumpHeld);
    }

    private void ClearSessionFiles()
    {
        foreach (var path in new[]
        {
            _paths.ChunkViewIntent,
            _paths.PlayerInputIntent, _paths.PlayerStateStream,
            _paths.WorldTimeIntent, _paths.UiActionIntent,
            _paths.UiActionIntent + ".ack",
        })
        {
            TryDelete(path);
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

}
