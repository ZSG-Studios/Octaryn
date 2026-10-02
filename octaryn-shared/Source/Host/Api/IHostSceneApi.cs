namespace Octaryn.Shared.Host.Api;

public readonly record struct HostSceneTicket(ulong Id, ulong Generation)
{
    public bool IsValid => Id != 0 && Generation != 0;
}

public enum HostScenePreparation : uint { Queued, Running, CpuPrepared, Failed, Canceled }
public enum HostScenePublication : uint { Unpublished, Published, Retiring }

public readonly record struct HostSceneProgress(
    HostScenePreparation Preparation, HostScenePublication Publication,
    ulong Completed, ulong Total, ulong RetainedBytes);

// Preparation verifies an immutable CPU metadata/resource snapshot. GPU
// publication belongs to the renderer and is never implied by CpuPrepared.
public interface IHostSceneApi : IDisposable
{
    const int MaximumTickets = 8;
    bool BeginPrepare(string assetId, out HostSceneTicket ticket, out string error);
    bool TryGetStatus(HostSceneTicket ticket, out HostSceneProgress progress, out string error);
    bool Cancel(HostSceneTicket ticket);
    bool Release(HostSceneTicket ticket);
}
