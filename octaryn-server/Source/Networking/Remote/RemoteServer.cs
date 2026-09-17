using System.Net;
using System.Net.Sockets;
using LiteEntitySystem;
using LiteEntitySystem.Transport;
using LiteNetLib;
using Octaryn.Server.Host;
using Octaryn.Server.Modules;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

// Standalone dedicated server transport. Binds a LiteNetLib listener, hosts a
// LiteEntitySystem ServerEntityManager with one authoritative remote player
// session, and keeps ticking until shutdown is requested through the shutdown
// file or console interrupt.
internal sealed class RemoteServer : IDisposable
{
    private const byte LesHeaderByte = 0x4F;
    private const byte LesTickrate = 60;

    private readonly ModuleActivator _gameModule;
    private readonly RemoteSession _session;
    private readonly uint _intervalMilliseconds;
    private readonly Func<bool> _shutdownFileRequested;
    private readonly EventBasedLiteNetListener _listener = new();
    private readonly LiteNetManager _manager;
    private readonly ServerEntityManager _entityManager;
    private bool _cancelRequested;
    private bool _disposed;

    public RemoteServer(
        ModuleActivator gameModule,
        string sessionDirectory,
        uint intervalMilliseconds,
        Func<bool> shutdownFileRequested)
    {
        _gameModule = gameModule;
        _intervalMilliseconds = intervalMilliseconds;
        _shutdownFileRequested = shutdownFileRequested;
        _manager = new LiteNetManager(_listener);
        _entityManager = new ServerEntityManager(
            CreateTypesMap(), LesHeaderByte, LesTickrate,
            ServerSendRate.EqualToFPS, MaxHistorySize.Size16);
        _session = new RemoteSession(gameModule, _entityManager, _manager, sessionDirectory);
        _listener.ConnectionRequestEvent += request =>
        {
            if (request.AcceptIfKey(RemoteProtocol.ConnectionKey) is null)
            {
                LiveDebugLog.Write("server_remote_connection rejected=1 reason=bad_key");
            }
        };
        _listener.PeerConnectedEvent += peer =>
        {
            if (_session.HasPeer)
            {
                LiveDebugLog.Write($"server_remote_peer rejected=1 reason=server-full endpoint={peer}");
                _manager.DisconnectPeer(peer);
                return;
            }

            var player = _entityManager.AddPlayer(new LiteNetLibNetPeer(peer, true));
            _session.TryAttach(player);
        };
        _listener.PeerDisconnectedEvent += (peer, info) =>
        {
            if (_session.OwnsPeer(peer)) _session.Detach($"{info.Reason}");
        };
        _listener.NetworkReceiveEvent += (peer, reader, _) =>
        {
            var packet = reader.GetRemainingBytes();
            if (packet.Length != 0 && packet[0] == PlayerCommandPacket.Header)
            {
                if (_session.OwnsPeer(peer)) _session.ReceivePlayerCommands(packet);
                return;
            }
            if (peer.GetLiteNetLibNetPeerFromTag() is not { } transport ||
                _entityManager.Deserialize(transport, packet) != DeserializeResult.Done)
            {
                LiveDebugLog.Write($"server_remote_deserialize failed=1 endpoint={peer}");
            }
        };
        _listener.NetworkErrorEvent += (endpoint, error) =>
            LiveDebugLog.Write($"server_remote_network_error endpoint={endpoint} error={error}");
        Console.CancelKeyPress += OnCancelKey;
    }

    public int Execute(string host, int port)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(host) || host is "0.0.0.0" or "*" or "::")
            {
                if (!_manager.Start(port))
                {
                    throw new InvalidOperationException("listener refused the port");
                }
            }
            else
            {
                var address = ResolveBindAddress(host);
                if (address is null)
                {
                    throw new InvalidOperationException("could not resolve the address");
                }

                if (address.AddressFamily == AddressFamily.InterNetworkV6)
                {
                    _manager.IPv6Enabled = true;
                    if (!_manager.Start(IPAddress.IPv6Any, address, port))
                    {
                        throw new InvalidOperationException("listener refused the address");
                    }
                }
                else
                {
                    _manager.IPv6Enabled = false;
                    if (!_manager.Start(address, IPAddress.IPv6Any, port))
                    {
                        throw new InvalidOperationException("listener refused the address");
                    }
                }
            }
        }
        catch (Exception ex)
        {
            LiveDebugLog.Write($"server_remote_listen active=0 endpoint={host}:{port} error={ex.GetType().Name}");
            return -3;
        }

        LiveDebugLog.Write($"server_remote_listening active=1 endpoint={host}:{_manager.LocalPort}");
        Console.WriteLine("octaryn_server_ready=1");
        var result = NativeHostPolicyLibrary.RunLiveStreamLoop(_intervalMilliseconds, Step);
        return result == 1 && (_cancelRequested || _shutdownFileRequested()) ? 0 : result;
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        Console.CancelKeyPress -= OnCancelKey;
        _session.Dispose();
        try
        {
            if (_manager.IsRunning)
            {
                _manager.Stop();
            }
        }
        catch (Exception ex)
        {
            LiveDebugLog.Write($"server_remote_stop_warning error={ex.GetType().Name}");
        }
    }

    internal static EntityTypesMap CreateTypesMap()
    {
        var map = new EntityTypesMap<RemoteEntityType>();
        map.Register(RemoteEntityType.Session, parameters => new SessionEntity(parameters));
        map.Register(RemoteEntityType.Controller, parameters => new SessionController(parameters));
        return map;
    }

    private static IPAddress? ResolveBindAddress(string host)
    {
        if (IPAddress.TryParse(host, out var parsed))
        {
            return parsed;
        }

        try
        {
            var entry = Dns.GetHostEntry(host);
            foreach (var address in entry.AddressList)
            {
                if (address.AddressFamily is AddressFamily.InterNetwork or AddressFamily.InterNetworkV6)
                {
                    return address;
                }
            }
        }
        catch
        {
            // Host resolution failed; fall through to null.
        }

        return null;
    }

    private int Step()
    {
        if (_cancelRequested || _shutdownFileRequested())
        {
            LiveDebugLog.Write("server_remote_shutdown requested=1");
            Console.WriteLine("octaryn_server_shutdown=1");
            return 1;
        }

        _manager.PollEvents();
        _entityManager.Update();
        _session.Step();
        return 0;
    }

    private void OnCancelKey(object? sender, ConsoleCancelEventArgs args)
    {
        args.Cancel = true;
        _cancelRequested = true;
    }
}
