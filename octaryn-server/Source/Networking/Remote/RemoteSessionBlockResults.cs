using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Networking.Remote;

internal sealed partial class RemoteSession
{
    private byte[]? _lastBlockResults;

    private void PublishBlockResults(SessionEntity entity)
    {
        var path = Path.Combine(_runtimeDirectory, "block_results.json");
        try
        {
            if (!File.Exists(path) || new FileInfo(path).Length > RemoteProtocol.MaxIntentTextBytes) return;
            var payload = File.ReadAllBytes(path);
            if (_lastBlockResults is not null && payload.AsSpan().SequenceEqual(_lastBlockResults)) return;
            entity.SendBlockResults(payload);
            _lastBlockResults = payload;
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
