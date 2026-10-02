using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host.Api;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct HostApplicationApiTable
{
    public uint Version, Size;
    public delegate* unmanaged[Cdecl]<int> RequestExit;
}

internal sealed unsafe partial class NativeHostApiProvider
{
    public IHostApplicationApi? GetApplicationApi()
    {
        if (_query is null) return null;
        var table = (HostApplicationApiTable*)_query(HostApiTableIds.Application, HostApiTableIds.ApplicationVersion);
        return table is null || table->Version < 1 || table->Size < sizeof(HostApplicationApiTable) ||
            table->RequestExit is null ? null : new NativeApplicationApi(table);
    }
    private sealed class NativeApplicationApi(HostApplicationApiTable* table) : IHostApplicationApi
    {
        public bool RequestExit() => table->RequestExit() == 0;
    }
}
