using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal abstract class ScenePreparationApi : IHostSceneApi
{
    private readonly object _sync = new();
    private readonly DeclaredSceneAssets _assets;
    private readonly HashSet<HostSceneTicket> _tickets = [];
    private readonly int _maximumTickets;
    private bool _disposed;

    protected ScenePreparationApi(GameModuleManifest manifest, string root, int maximumTickets)
    {
        _assets = new DeclaredSceneAssets(manifest, root);
        _maximumTickets = maximumTickets;
    }

    public bool BeginPrepare(string assetId, out HostSceneTicket ticket, out string error)
    {
        ticket = default;
        error = "Scene preparation owner is disposed or at its ticket limit.";
        lock (_sync)
        {
            if (_disposed || _tickets.Count >= _maximumTickets) return false;
            if (!_assets.TryResolve(assetId, out var source, out error)) return false;
            if (!Begin(source, assetId, out var result) || !result.IsValid)
            {
                error = "Host scene preparation could not admit this request.";
                return false;
            }
            if (!_tickets.Add(result))
            {
                error = "Host scene preparation returned a duplicate ticket.";
                return false;
            }
            ticket = result;
            error = string.Empty;
            return true;
        }
    }

    public bool TryGetStatus(HostSceneTicket ticket, out HostSceneProgress progress, out string error)
    {
        progress = default;
        error = "Scene ticket is stale, released, or belongs to another owner.";
        lock (_sync)
        {
            if (_disposed || !_tickets.Contains(ticket)) return false;
            if (!Query(ticket, out var result) || (uint)result.Preparation > (uint)HostScenePreparation.Canceled ||
                result.Publication != HostScenePublication.Unpublished || result.Completed > result.Total)
            {
                error = "Host scene preparation returned unavailable or invalid progress.";
                return false;
            }
            progress = result;
            error = result.Preparation == HostScenePreparation.Failed ? FailureReason(ticket) : string.Empty;
            return true;
        }
    }

    public bool Cancel(HostSceneTicket ticket)
    {
        lock (_sync) return !_disposed && _tickets.Contains(ticket) && CancelRequest(ticket);
    }

    public bool Release(HostSceneTicket ticket)
    {
        lock (_sync)
        {
            if (_disposed || !_tickets.Contains(ticket) || !ReleaseRequest(ticket)) return false;
            _tickets.Remove(ticket);
            return true;
        }
    }

    public void Dispose()
    {
        lock (_sync)
        {
            if (_disposed) return;
            _disposed = true;
            try
            {
                foreach (var ticket in _tickets)
                {
                    CancelRequest(ticket);
                    ReleaseRequest(ticket);
                }
            }
            finally
            {
                _tickets.Clear();
                DestroyOwner();
            }
        }
    }

    protected abstract bool Begin(string source, string asset, out HostSceneTicket ticket);
    protected abstract bool Query(HostSceneTicket ticket, out HostSceneProgress progress);
    protected abstract bool CancelRequest(HostSceneTicket ticket);
    protected abstract bool ReleaseRequest(HostSceneTicket ticket);
    protected abstract void DestroyOwner();
    protected virtual string FailureReason(HostSceneTicket ticket) => "Host scene preparation failed.";
}
