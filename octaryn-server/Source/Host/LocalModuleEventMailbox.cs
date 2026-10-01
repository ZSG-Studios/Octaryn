using System.Text.Json;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Host;

// Retain events until the presentation consumer acknowledges their sequence.
internal sealed class LocalModuleEventMailbox : Networking.Remote.IServerReplicationChannel
{
    private sealed record Entry(ulong seq, ulong id, ulong kind, ulong p1, ulong p2);
    private readonly string _path;
    private readonly List<Entry> _pending = new();
    private ulong _sequence;
    private string? _published;
    public int AvailableChangeCapacity => 256 - _pending.Count;

    public LocalModuleEventMailbox(string path) => _path = path;

    public bool Broadcast(in ModuleEventData data)
    {
        Flush();
        if (AvailableChangeCapacity == 0) return false;
        _pending.Add(new Entry(++_sequence, data.EventId, data.Payload0, data.Payload1, data.Payload2));
        Flush();
        return true;
    }

    public void Flush()
    {
        try
        {
            var ackPath = _path + ".ack";
            if (File.Exists(ackPath) && ulong.TryParse(File.ReadAllText(ackPath), out var ack) && ack <= _sequence)
                _pending.RemoveAll(entry => entry.seq <= ack);
            var payload = JsonSerializer.Serialize(new { version = 1, events = _pending });
            if (payload == _published) return;
            File.WriteAllText(_path + ".tmp", payload);
            File.Move(_path + ".tmp", _path, overwrite: true);
            _published = payload;
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            // The retained journal is retried on the next authority tick.
        }
    }
}
