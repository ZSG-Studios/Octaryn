using System.Runtime.InteropServices;

namespace Octaryn.Engine.Api;

public unsafe readonly struct OctarynNativeBridge
{
    private readonly delegate* unmanaged[Cdecl]<OctarynNativeCommand*, int> _enqueueCommand;

    private OctarynNativeBridge(delegate* unmanaged[Cdecl]<OctarynNativeCommand*, int> enqueueCommand)
    {
        _enqueueCommand = enqueueCommand;
    }

    public bool IsValid => _enqueueCommand is not null;

    public static OctarynNativeBridge Create(OctarynNativeApi* api)
    {
        if (api is null || api->Version != OctarynNativeApi.VersionValue || api->Size != (uint)sizeof(OctarynNativeApi))
        {
            return default;
        }

        return new OctarynNativeBridge(api->EnqueueCommand);
    }

    public bool Enqueue(OctarynNativeCommand command)
    {
        if (_enqueueCommand is null)
        {
            return false;
        }

        return _enqueueCommand(&command) != 0;
    }
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct OctarynNativeApi
{
    public const uint VersionValue = 1;

    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<OctarynNativeCommand*, int> EnqueueCommand;
}
