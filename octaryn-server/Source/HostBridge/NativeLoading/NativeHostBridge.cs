using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;

namespace Octaryn.Server.HostBridge;

internal unsafe readonly struct NativeHostBridge : IHostCommandSink
{
    private readonly delegate* unmanaged[Cdecl]<HostCommand*, int> _enqueueHostCommand;
    private readonly delegate* unmanaged[Cdecl]<ServerSnapshotHeader*, int> _publishServerSnapshot;
    private readonly delegate* unmanaged[Cdecl]<ClientCommandFrame*, int> _pollClientCommands;

    private NativeHostBridge(
        delegate* unmanaged[Cdecl]<HostCommand*, int> enqueueHostCommand,
        delegate* unmanaged[Cdecl]<ServerSnapshotHeader*, int> publishServerSnapshot,
        delegate* unmanaged[Cdecl]<ClientCommandFrame*, int> pollClientCommands)
    {
        _enqueueHostCommand = enqueueHostCommand;
        _publishServerSnapshot = publishServerSnapshot;
        _pollClientCommands = pollClientCommands;
    }

    public bool IsValid =>
        _enqueueHostCommand is not null &&
        _publishServerSnapshot is not null &&
        _pollClientCommands is not null;

    public static NativeHostBridge Create(NativeHostApi* api)
    {
        if (api is null || api->Version != NativeHostApi.VersionValue || api->Size != NativeHostApi.SizeValue)
        {
            return default;
        }

        return new NativeHostBridge(
            api->EnqueueHostCommand,
            api->PublishServerSnapshot,
            api->PollClientCommands);
    }

    public bool Enqueue(HostCommand command)
    {
        if (_enqueueHostCommand is null)
        {
            return false;
        }

        return _enqueueHostCommand(&command) != 0;
    }

    public bool Publish(ServerSnapshotHeader snapshot)
    {
        if (_publishServerSnapshot is null)
        {
            return false;
        }

        return _publishServerSnapshot(&snapshot) != 0;
    }

    public bool Poll(ClientCommandFrame commandFrame)
    {
        if (_pollClientCommands is null)
        {
            return false;
        }

        return _pollClientCommands(&commandFrame) != 0;
    }
}
