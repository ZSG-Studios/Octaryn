using System.Text;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal unsafe sealed class NativeScenePreparationApi : ScenePreparationApi
{
    private readonly NativeSceneLoadingLibrary _library;
    private nint _owner;

    private NativeScenePreparationApi(NativeSceneLoadingLibrary library, nint owner, GameModuleManifest manifest, string root)
        : base(manifest, root, IHostSceneApi.MaximumTickets)
    {
        _library = library;
        _owner = owner;
    }

    internal static IHostSceneApi? TryCreate(GameModuleManifest manifest, string root, NativeScheduleRuntime? scheduler)
    {
        if (scheduler is null) return null;
        var library = NativeSceneLoadingLibrary.TryLoad();
        if (library is null) return null;
        var moduleBytes = Encoding.UTF8.GetBytes(manifest.ModuleId + '\0');
        var rootBytes = Encoding.UTF8.GetBytes(Path.GetFullPath(root) + '\0');
        nint owner;
        fixed (byte* module = moduleBytes, rootPointer = rootBytes) owner = library.Create(module, rootPointer, scheduler.Handle);
        if (owner == 0) return null;
        try { return new NativeScenePreparationApi(library, owner, manifest, root); }
        catch { library.Destroy(owner); throw; }
    }

    protected override bool Begin(string source, string asset, out HostSceneTicket ticket)
    {
        var sourceBytes = Encoding.UTF8.GetBytes(source + '\0');
        HostSceneTicketNative native = default;
        int status;
        fixed (byte* path = sourceBytes) status = _library.Begin(_owner, path, &native);
        ticket = native.Managed;
        return status == 0;
    }

    protected override bool Query(HostSceneTicket ticket, out HostSceneProgress progress)
    {
        var native = HostSceneTicketNative.From(ticket);
        HostSceneProgressNative result = default;
        var status = _library.Query(_owner, &native, &result);
        progress = result.Managed;
        return status == 0;
    }

    protected override bool CancelRequest(HostSceneTicket ticket)
    {
        var native = HostSceneTicketNative.From(ticket);
        return _library.Cancel(_owner, &native) == 0;
    }

    protected override bool ReleaseRequest(HostSceneTicket ticket)
    {
        var native = HostSceneTicketNative.From(ticket);
        return _library.Release(_owner, &native) == 0;
    }

    protected override void DestroyOwner()
    {
        if (_owner == 0) return;
        _library.Destroy(_owner);
        _owner = 0;
    }

    protected override string FailureReason(HostSceneTicket ticket)
    {
        var native = HostSceneTicketNative.From(ticket);
        Span<byte> bytes = stackalloc byte[1025];
        bytes.Clear();
        fixed (byte* destination = bytes)
            if (_library.Error(_owner, &native, destination, (uint)bytes.Length) != 0) return base.FailureReason(ticket);
        var count = bytes.IndexOf((byte)0);
        return count > 0 ? Encoding.UTF8.GetString(bytes[..count]) : base.FailureReason(ticket);
    }
}
