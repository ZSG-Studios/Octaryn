using System.Runtime.InteropServices;
using System.Text;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 16)]
internal struct HostSceneTicketNative
{
    public ulong Id, Generation;
    public readonly HostSceneTicket Managed => new(Id, Generation);
    public static HostSceneTicketNative From(HostSceneTicket value) => new() { Id = value.Id, Generation = value.Generation };
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
internal struct HostSceneProgressNative
{
    public uint Preparation, Publication;
    public ulong Completed, Total, RetainedBytes;
    public readonly HostSceneProgress Managed => new((HostScenePreparation)Preparation,
        (HostScenePublication)Publication, Completed, Total, RetainedBytes);
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 48)]
internal unsafe struct HostSceneApiTable
{
    public uint Version, Size;
    public delegate* unmanaged[Cdecl]<byte*, byte*, HostSceneTicketNative*, int> BeginPrepare;
    public delegate* unmanaged[Cdecl]<byte*, HostSceneTicketNative*, HostSceneProgressNative*, int> Query;
    public delegate* unmanaged[Cdecl]<byte*, HostSceneTicketNative*, int> Cancel, Release;
    public uint MaximumTickets, Reserved;
}

internal unsafe sealed partial class NativeHostApiProvider
{
    public IHostSceneApi? GetSceneApi(GameModuleManifest manifest, string moduleRoot)
    {
        if (_query is null) return null;
        var table = (HostSceneApiTable*)_query(HostApiTableIds.Scene, HostApiTableIds.SceneVersion);
        if (table is null || table->Version < HostApiTableIds.SceneVersion || table->Size < sizeof(HostSceneApiTable) ||
            table->BeginPrepare is null || table->Query is null || table->Cancel is null || table->Release is null ||
            table->MaximumTickets == 0 || table->MaximumTickets > IHostSceneApi.MaximumTickets || table->Reserved != 0)
            return null;
        return new NativeSceneTableApi(table, manifest, moduleRoot);
    }

    private sealed class NativeSceneTableApi : ScenePreparationApi
    {
        private readonly HostSceneApiTable* _table;
        private readonly byte[] _module;

        internal NativeSceneTableApi(HostSceneApiTable* table, GameModuleManifest manifest, string root)
            : base(manifest, root, checked((int)table->MaximumTickets))
        {
            _table = table;
            _module = Encoding.UTF8.GetBytes(manifest.ModuleId + '\0');
        }

        protected override bool Begin(string source, string asset, out HostSceneTicket ticket)
        {
            var id = Encoding.UTF8.GetBytes(asset + '\0');
            HostSceneTicketNative native = default;
            int status;
            fixed (byte* module = _module, assetPointer = id) status = _table->BeginPrepare(module, assetPointer, &native);
            ticket = native.Managed;
            return status == 0;
        }

        protected override bool Query(HostSceneTicket ticket, out HostSceneProgress progress)
        {
            var native = HostSceneTicketNative.From(ticket);
            HostSceneProgressNative state = default;
            int status;
            fixed (byte* module = _module) status = _table->Query(module, &native, &state);
            progress = state.Managed;
            return status == 0;
        }

        protected override bool CancelRequest(HostSceneTicket ticket)
        {
            var native = HostSceneTicketNative.From(ticket);
            fixed (byte* module = _module) return _table->Cancel(module, &native) == 0;
        }

        protected override bool ReleaseRequest(HostSceneTicket ticket)
        {
            var native = HostSceneTicketNative.From(ticket);
            fixed (byte* module = _module) return _table->Release(module, &native) == 0;
        }

        protected override void DestroyOwner() { }
    }
}
