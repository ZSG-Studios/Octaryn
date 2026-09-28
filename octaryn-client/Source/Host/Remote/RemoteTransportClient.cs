using LiteEntitySystem;
using LiteEntitySystem.Transport;
using LiteNetLib;
using LiteNetLib.Utils;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

// Host-owned LiteEntitySystem transport. Native presentation exchanges typed
// poses, bounded command batches, intent journals and ACKs in memory; only the
// server consumes commands and advances authoritative state.
internal sealed partial class RemoteTransportClient : IDisposable
{
    private const byte LesHeaderByte = 0x4F;

    private readonly object _mutex = new();
    private Thread? _thread;
    private EventBasedLiteNetListener? _listener;
    private LiteNetManager? _manager;
    private ClientEntityManager? _entityManager;
    private SessionEntity? _entity;
    private SessionController? _controller;
    private LiteNetPeer? _peer;
    private ManualResetEventSlim? _welcomeSignal;
    private string _endpoint = string.Empty;
    private sealed record TransportStatus(bool Running, string Message);
    private TransportStatus _publishedStatus = new(false, "stopped");
    private string _status = "stopped";
    private bool _welcomed;
    private bool _hasWelcomed;
    private bool _helloSent;
    private bool _stopRequested;
    private bool _fatalError;
    private bool _disposed;
    private ulong? _publishedPoseTick;
    private ulong _sessionHigh, _sessionLow;

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
                SetStatusLocked($"error: invalid endpoint {endpoint}");
                return false;
            }

            var session = Guid.NewGuid().ToByteArray();
            _sessionHigh = BitConverter.ToUInt64(session, 0);
            _sessionLow = BitConverter.ToUInt64(session, 8);
            _receivedEventSequence = 0;
            _hasWelcomed = false;
            _endpoint = $"{host}:{port}";
            SetStatusLocked($"connecting to {_endpoint}");
            _welcomed = false;
            _helloSent = false;
            _stopRequested = false;
            _fatalError = false;
            _publishedPoseTick = null;
            Volatile.Write(ref _commandBatch, null);
            Volatile.Write(ref _latestPose, null);
            ResetTiming();
            Array.Clear(_intentSlots);
            Array.Clear(_sentIntents);
            Volatile.Write(ref _actionAcknowledgement, null);
            _welcomeSignal = new ManualResetEventSlim(false);
            _thread = new Thread(() => Run(host, port)) { IsBackground = true, Name = "octaryn-remote-transport" };
            _thread.Start();
            PublishStatusLocked();
        }

        if (welcomeTimeoutMilliseconds == 0) return true;
        if (_welcomeSignal.Wait(TimeSpan.FromMilliseconds(welcomeTimeoutMilliseconds)))
        {
            lock (_mutex)
            {
                return _welcomed;
            }
        }

        lock (_mutex)
        {
            SetStatusLocked($"error: timed out connecting to {_endpoint}");
            return false;
        }
    }

    public void Stop()
    {
        Thread? thread;
        lock (_mutex)
        {
            _stopRequested = true;
            SetStatusLocked("stopped");
            thread = _thread;
        }

        if (thread is not null && !thread.Join(TimeSpan.FromSeconds(5)))
            throw new TimeoutException("Remote transport did not stop; its owned state remains alive.");
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
            Array.Clear(_sentIntents);
            _moduleEvents.Clear();
            _welcomeSignal?.Dispose();
            _welcomeSignal = null;
        }
    }

    // Frame-thread reads never wait behind network processing or filesystem work.
    public bool IsRunning => Volatile.Read(ref _publishedStatus).Running;
    public string Status => Volatile.Read(ref _publishedStatus).Message;

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
                    Console.Error.WriteLine($"remote_transport_crash {ex.GetType().Name}: {ex.Message} | {ex.StackTrace}");
                    Fail($"error: transport failed: {ex.GetType().Name}");
                    break;
                }

                lock (_mutex)
                {
                    if (_stopRequested || _fatalError)
                        break;
                }

                // Only this thread owns LES and the mailbox queues. Stop joins
                // it before clearing them; no native-frame lock covers I/O.
                if (_peer is not null && _entityManager is not null)
                {
                    _entityManager.Update();
                    if (_welcomed)
                    {
                        SyncIntents();
                        PublishPose();
                    }
                }
                else if (DateTime.UtcNow >= nextReconnect)
                {
                    nextReconnect = DateTime.UtcNow.AddSeconds(2);
                    SetStatus($"connecting to {_endpoint}");
                    manager.Connect(host, port, RemoteProtocol.ConnectionKey);
                }

                Thread.Sleep(5);
            }
        }
        finally
        {
            manager.Stop();
            lock (_mutex)
            {
                _stopRequested = true;
                PublishStatusLocked();
            }
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
            _worldItems.ResetConnection();
            Volatile.Write(ref _commandBatch, null);
            Volatile.Write(ref _latestPose, null);
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
            entity.ModuleEventReceived += OnModuleEvent;
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
                controller.SendHello(RemoteProtocol.Version, _sessionHigh, _sessionLow);
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
            Array.Clear(_sentIntents);
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
            if (_peer != peer) return;
            entityManager = _entityManager;
        }

        if (entityManager is null)
        {
            return;
        }

        try
        {
            var bytes = reader.GetRemainingBytes();
            if (bytes.Length != 0 && bytes[0] == WorldItemPacket.Header)
            {
                OnWorldItems(peer, bytes);
                return;
            }
            if (entityManager.Deserialize(bytes) == DeserializeResult.Error)
            {
                SetStatus("error: bad server state");
            }
        }
        catch (Exception ex)
        {
            SetStatus($"error: bad server message: {ex.GetType().Name}");
        }
    }

    private void OnWelcome(SessionWelcome welcome)
    {
        lock (_mutex)
        {
            if (welcome.Version != RemoteProtocol.Version || welcome.SessionHigh != _sessionHigh || welcome.SessionLow != _sessionLow ||
                (_hasWelcomed && welcome.Resumed == 0))
            {
                _fatalError = true;
                SetStatusLocked($"error: authority session or protocol changed at {_endpoint}; start a new session");
                _welcomeSignal?.Set();
                return;
            }

            _welcomed = true;
            _hasWelcomed = true;
            Array.Clear(_sentIntents);
            SetStatusLocked($"connected to {_endpoint}");
            _welcomeSignal?.Set();
        }
        Console.Error.WriteLine($"remote_session_welcome resumed={welcome.Resumed} ack={welcome.AcknowledgedInput}");
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
        PublishStatusLocked();
    }

    private void PublishStatusLocked()
    {
        Volatile.Write(ref _publishedStatus, new TransportStatus(
            _thread is not null && _thread.IsAlive && !_stopRequested && !_fatalError, _status));
    }

    private void Fail(string status)
    {
        lock (_mutex)
        {
            _fatalError = true;
            SetStatusLocked(status);
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
