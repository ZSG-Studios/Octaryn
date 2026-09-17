using LiteEntitySystem;
using LiteEntitySystem.Transport;
using LiteNetLib;
using LiteNetLib.Utils;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

// Host-owned LiteEntitySystem transport backing a remote client session. It
// mirrors the local session mailbox files across the network: intent files
// written by the native frame loop use entity requests except for resending
// movement datagrams. State received from the server (pose SyncVars,
// snapshot/ack RPCs) is written back for the native frame loop to read.
// Presentation keeps consuming the same files, so no client authority is
// introduced: edits only take effect through server acknowledgements.
internal sealed partial class RemoteTransportClient : IDisposable
{
    private const byte LesHeaderByte = 0x4F;
    private const string ChunkViewFile = "chunk_view.json";
    private const string ChunkStreamBinFile = "chunk_stream.json.bin";
    private const string PlayerInputFile = "player_input.json";
    private const string PlayerStateFile = "player_state.json";
    private const string BlockInteractionFile = "block_interaction.json";
    private const string WorldTimeFile = "world_time.json";
    private const string WorldItemsIntentFile = "world_items.intent";
    private const string WorldItemsSnapshotFile = "world_items.snapshot";

    private readonly object _mutex = new();
    private readonly Dictionary<string, byte[]> _sent = new();
    private Thread? _thread;
    private EventBasedLiteNetListener? _listener;
    private LiteNetManager? _manager;
    private ClientEntityManager? _entityManager;
    private SessionEntity? _entity;
    private SessionController? _controller;
    private LiteNetPeer? _peer;
    private ManualResetEventSlim? _welcomeSignal;
    private string _endpoint = string.Empty;
    private string _runtimeDirectory = string.Empty;
    private string _status = "stopped";
    private bool _welcomed;
    private bool _helloSent;
    private bool _stopRequested;
    private bool _fatalError;
    private bool _disposed;
    private ulong? _publishedPoseTick;

    public bool Start(string endpoint, string runtimeDirectory, int welcomeTimeoutMilliseconds)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        lock (_mutex)
        {
            if (_thread is not null)
            {
                return false;
            }

            if (!TryParseEndpoint(endpoint, out var host, out var port))
            {
                _status = $"error: invalid endpoint {endpoint}";
                return false;
            }

            _endpoint = $"{host}:{port}";
            _runtimeDirectory = runtimeDirectory;
            _status = $"connecting to {_endpoint}";
            _welcomed = false;
            _helloSent = false;
            _stopRequested = false;
            _fatalError = false;
            _publishedPoseTick = null;
            ResetTiming();
            _pendingWrites.Clear();
            _pendingBlockAcks.Clear();
            _pendingWriteBytes = 0;
            _welcomeSignal = new ManualResetEventSlim(false);
            _thread = new Thread(() => Run(host, port)) { IsBackground = true, Name = "octaryn-remote-transport" };
            _thread.Start();
        }

        if (_welcomeSignal.Wait(TimeSpan.FromMilliseconds(welcomeTimeoutMilliseconds)))
        {
            lock (_mutex)
            {
                return _welcomed;
            }
        }

        lock (_mutex)
        {
            _status = $"error: timed out connecting to {_endpoint}";
            return false;
        }
    }

    public void Stop()
    {
        Thread? thread;
        lock (_mutex)
        {
            _stopRequested = true;
            _status = "stopped";
            thread = _thread;
        }

        thread?.Join(TimeSpan.FromSeconds(5));
        lock (_mutex)
        {
            _thread = null;
            _manager = null;
            _listener = null;
            _entityManager = null;
            _entity = null;
            _controller = null;
            _peer = null;
            _welcomed = false;
            _helloSent = false;
            _sent.Clear();
            _welcomeSignal?.Dispose();
            _welcomeSignal = null;
        }
    }

    public bool IsRunning
    {
        get
        {
            lock (_mutex)
            {
                return _thread is not null && _thread.IsAlive && !_fatalError;
            }
        }
    }

    public string Status
    {
        get
        {
            lock (_mutex)
            {
                return _status;
            }
        }
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        Stop();
        _welcomeSignal?.Dispose();
    }

    private void Run(string host, int port)
    {
        var listener = new EventBasedLiteNetListener();
        var manager = new LiteNetManager(listener);
        lock (_mutex)
        {
            _listener = listener;
            _manager = manager;
        }

        listener.PeerConnectedEvent += OnPeerConnected;
        listener.PeerDisconnectedEvent += (peer, info) => OnPeerDisconnected(peer, info);
        listener.NetworkReceiveEvent += (peer, reader, _) => OnReceive(peer, reader);
        listener.NetworkErrorEvent += (endpoint, error) => SetStatus($"network error: {error}");

        try
        {
            manager.Start();
            manager.Connect(host, port, RemoteProtocol.ConnectionKey);
        }
        catch (Exception ex)
        {
            Fail($"error: cannot reach {_endpoint}: {ex.GetType().Name}");
            manager.Stop();
            return;
        }

        var nextReconnect = DateTime.UtcNow;
        try
        {
            while (true)
            {
                lock (_mutex)
                {
                    if (_stopRequested)
                    {
                        break;
                    }
                }

                try
                {
                    manager.PollEvents();
                }
                catch (Exception ex)
                {
                    Fail($"error: transport failed: {ex.GetType().Name}");
                    break;
                }

                lock (_mutex)
                {
                    if (_stopRequested || _fatalError)
                    {
                        break;
                    }

                    if (_peer is not null && _entityManager is not null)
                    {
                        _entityManager.Update();
                        if (_welcomed)
                        {
                            FlushMailboxes();
                            SyncFiles();
                            PublishPose();
                        }
                    }
                    else if (DateTime.UtcNow >= nextReconnect)
                    {
                        nextReconnect = DateTime.UtcNow.AddSeconds(2);
                        SetStatusLocked($"connecting to {_endpoint}");
                        manager.Connect(host, port, RemoteProtocol.ConnectionKey);
                    }
                }

                Thread.Sleep(5);
            }
        }
        finally
        {
            manager.Stop();
        }
    }

    private void OnPeerConnected(LiteNetPeer peer)
    {
        var entityManager = new ClientEntityManager(
            CreateTypesMap(), new LiteNetLibNetPeer(peer, true),
            LesHeaderByte, MaxHistorySize.Size16)
        {
            // PoseHistory buffers presentation; LES retains its adaptive jitter allowance.
            PreferredBufferTimeLowest = 0f,
            PreferredBufferTimeHighest = 0f,
        };
        entityManager.GetEntities<SessionEntity>().SubscribeToConstructed(OnSessionEntityConstructed, true);
        entityManager.GetControllers<SessionController>().SubscribeToConstructed(OnSessionControllerConstructed, true);
        lock (_mutex)
        {
            _peer = peer;
            ResetTiming();
            _welcomed = false;
            _helloSent = false;
            _entity = null;
            _controller = null;
            _publishedPoseTick = null;
            _entityManager = entityManager;
            SetStatusLocked($"handshake with {_endpoint}");
        }
    }

    private void OnSessionEntityConstructed(SessionEntity entity)
    {
        lock (_mutex)
        {
            if (_entityManager is null || _peer is null)
            {
                return;
            }

            _entity = entity;
            entity.WelcomeReceived += OnWelcome;
            entity.SnapshotReceived += payload => QueueMailbox(ChunkStreamBinFile, payload);
            entity.ItemSnapshotReceived += payload => QueueMailbox(WorldItemsSnapshotFile, payload);
            entity.BlockResultsReceived += payload => QueueMailbox("block_results.json", payload);
            entity.BlockAckReceived += OnBlockAck;
        }
    }

    private void OnSessionControllerConstructed(SessionController controller)
    {
        lock (_mutex)
        {
            if (_entityManager is null || _peer is null || !controller.IsLocalControlled)
            {
                return;
            }

            _controller = controller;
            if (!_helloSent)
            {
                _helloSent = true;
                controller.SendHello(RemoteProtocol.Version);
            }
        }
    }

    private void OnPeerDisconnected(LiteNetPeer peer, DisconnectInfo info)
    {
        lock (_mutex)
        {
            if (_peer != peer)
            {
                return;
            }

            _peer = null;
            _entityManager = null;
            _entity = null;
            _controller = null;
            _welcomed = false;
            _helloSent = false;
            _sent.Clear();
            if (!_stopRequested && !_fatalError)
            {
                SetStatusLocked($"disconnected ({info.Reason}), reconnecting to {_endpoint}");
            }
        }
    }

    private void OnReceive(LiteNetPeer peer, NetPacketReader reader)
    {
        ClientEntityManager? entityManager;
        lock (_mutex)
        {
            entityManager = _entityManager;
        }

        if (entityManager is null)
        {
            return;
        }

        try
        {
            if (entityManager.Deserialize(reader.GetRemainingBytes()) == DeserializeResult.Error)
            {
                SetStatus("error: bad server state");
            }
        }
        catch (Exception ex)
        {
            SetStatus($"error: bad server message: {ex.GetType().Name}");
        }
    }

    private void OnWelcome(ulong version)
    {
        lock (_mutex)
        {
            if (version != RemoteProtocol.Version)
            {
                _fatalError = true;
                SetStatusLocked($"error: protocol mismatch with {_endpoint}");
                _welcomeSignal?.Set();
                return;
            }

            _welcomed = true;
            _sent.Clear();
            SetStatusLocked($"connected to {_endpoint}");
            _welcomeSignal?.Set();
        }
    }

    private void SetStatus(string status)
    {
        lock (_mutex)
        {
            SetStatusLocked(status);
        }
    }

    private void SetStatusLocked(string status)
    {
        _status = status;
    }

    private void Fail(string status)
    {
        lock (_mutex)
        {
            _fatalError = true;
            _status = status;
            _welcomeSignal?.Set();
        }
    }

    private static EntityTypesMap CreateTypesMap()
    {
        var map = new EntityTypesMap<RemoteEntityType>();
        map.Register(RemoteEntityType.Session, parameters => new SessionEntity(parameters));
        map.Register(RemoteEntityType.Controller, parameters => new SessionController(parameters));
        return map;
    }

    internal static bool TryParseEndpoint(string endpoint, out string host, out int port)
    {
        host = string.Empty;
        port = 0;
        if (string.IsNullOrWhiteSpace(endpoint))
        {
            return false;
        }

        var text = endpoint.Trim();
        if (text.StartsWith('['))
        {
            var bracket = text.IndexOf(']');
            if (bracket < 0 || bracket + 2 > text.Length || text[bracket + 1] != ':')
            {
                return false;
            }

            host = text[1..bracket];
            text = text[(bracket + 2)..];
        }
        else
        {
            var separator = text.LastIndexOf(':');
            if (separator < 0)
            {
                return false;
            }

            host = separator == 0 ? string.Empty : text[..separator];
            text = text[(separator + 1)..];
        }

        return !string.IsNullOrWhiteSpace(host) &&
            int.TryParse(text, out port) && port >= 1 && port <= 65535;
    }
}
