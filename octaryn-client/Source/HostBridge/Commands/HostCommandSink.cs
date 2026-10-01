using System.Runtime.InteropServices;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Client.HostBridge;

internal unsafe readonly struct HostCommandSink : IHostCommandSink
{
    private readonly delegate* unmanaged[Cdecl]<HostCommand*, int> _enqueueCommand;
    private readonly delegate* unmanaged[Cdecl]<uint, uint, void*> _queryHostApi;

    private HostCommandSink(
        delegate* unmanaged[Cdecl]<HostCommand*, int> enqueueCommand,
        delegate* unmanaged[Cdecl]<uint, uint, void*> queryHostApi)
    {
        _enqueueCommand = enqueueCommand;
        _queryHostApi = queryHostApi;
    }

    public bool IsValid => _enqueueCommand is not null;

    public static HostCommandSink Create(NativeHostApi* api)
    {
        if (api is null || api->Version != NativeHostApi.VersionValue || api->Size != NativeHostApi.SizeValue)
        {
            return default;
        }

        return new HostCommandSink(api->EnqueueCommand, api->QueryHostApi);
    }

    public IHostApiProvider? CreateApiProvider()
    {
        if (_queryHostApi is null)
        {
            return null;
        }

        var provider = new NativeHostApiProvider(_queryHostApi);
        return provider.IsValid ? provider : null;
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
