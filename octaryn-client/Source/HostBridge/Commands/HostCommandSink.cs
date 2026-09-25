using System.Runtime.InteropServices;
using Octaryn.Shared.Host;

namespace Octaryn.Client.HostBridge;

internal unsafe readonly struct HostCommandSink : IHostCommandSink
{
    private readonly delegate* unmanaged[Cdecl]<HostCommand*, int> _enqueueCommand;

    private HostCommandSink(delegate* unmanaged[Cdecl]<HostCommand*, int> enqueueCommand)
    {
        _enqueueCommand = enqueueCommand;
    }

    public bool IsValid => _enqueueCommand is not null;

    public static HostCommandSink Create(NativeHostApi* api)
    {
        if (api is null || api->Version != NativeHostApi.VersionValue || api->Size != NativeHostApi.SizeValue)
        {
            return default;
        }

        return new HostCommandSink(api->EnqueueCommand);
    }

    public bool Enqueue(HostCommand command)
    {
        if (_enqueueCommand is null)
        {
            return false;
        }

        return _enqueueCommand(&command) != 0;
    }
}
