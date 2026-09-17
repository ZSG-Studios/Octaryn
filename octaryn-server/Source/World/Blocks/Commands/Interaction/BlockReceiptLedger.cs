using System.Text.Json;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

// Server-thread owned. Reserve before consuming an intent; publish after save.
internal sealed class BlockReceiptLedger
{
    public const int Capacity = 256;
    private static readonly JsonSerializerOptions Json = new() { PropertyNamingPolicy = JsonNamingPolicy.CamelCase };
    private readonly string _resultsPath;
    private readonly string _ackPath;
    private readonly Func<BlockPosition, BlockId> _getBlock;
    private readonly Func<ulong> _revision;
    private readonly Dictionary<ulong, BlockPosition> _reserved = [];
    private readonly List<BlockReceipt> _staged = [];
    private readonly List<BlockReceipt> _published = [];
    private ulong _next, _ack, _publishedThrough, _highestCommand;
    private bool _mailboxDirty = true;
    public string Session { get; } = Guid.NewGuid().ToString("N");
    public string ResultsPath => _resultsPath;
    public bool IsKnownCommand(ulong commandID) => commandID != 0 && commandID <= _highestCommand;
    public int Count => _reserved.Count + _staged.Count + _published.Count;

    public BlockReceiptLedger(string runtimeDirectory, Func<BlockPosition, BlockId> getBlock, Func<ulong> revision)
    {
        _resultsPath = Path.Combine(runtimeDirectory, "block_results.json");
        _ackPath = Path.Combine(runtimeDirectory, "block_results_ack.json");
        _getBlock = getBlock;
        _revision = revision;
    }

    public bool TryReserve(ulong commandID, BlockPosition requestedCell)
    {
        if (commandID <= _highestCommand || Count >= Capacity || _reserved.ContainsKey(commandID) ||
            _staged.Any(r => r.CommandID == commandID) || _published.Any(r => r.CommandID == commandID)) return false;
        _reserved.Add(commandID, requestedCell);
        _highestCommand = commandID;
        return true;
    }

    public bool TryReserveAfterMovement(ulong commandID, BlockPosition requestedCell,
        ulong movementFrameID, ulong consumedMovementFrame)
    {
        if (movementFrameID > consumedMovementFrame) return false;
        return TryReserve(commandID, requestedCell);
    }

    public void Record(HostCommand command, BlockEditResult result)
    {
        if (!_reserved.TryGetValue(command.RequestId, out var requested)) return;
        if (command.A != requested.X || command.B != requested.Y || command.C != requested.Z) return;
        var cells = new List<BlockReceiptCell>();
        foreach (var edit in result.Changes)
            cells.Add(new(edit.Position.X, edit.Position.Y, edit.Position.Z, _getBlock(edit.Position).Value));
        if (!cells.Any(c => c.X == requested.X && c.Y == requested.Y && c.Z == requested.Z))
            cells.Add(new(requested.X, requested.Y, requested.Z, _getBlock(requested).Value));
        _staged.Add(new(++_next, command.RequestId, result.Applied, _revision(), cells));
        _reserved.Remove(command.RequestId);
    }

    public void Reject(HostCommand command) => Record(command, new(false, false, []));

    // Call only after SaveIfDirty returned successfully. The callback must save
    // all edits whose revision is <= durableRevision, or throw without returning.
    public void SaveAndPublish(Action save, ulong durableRevision)
    {
        save();
        var ready = _staged.TakeWhile(r => r.Revision <= durableRevision).ToArray();
        if (ready.Length == 0 && !_mailboxDirty) return;
        var batch = new BlockReceiptBatch(1, Session, _published.Concat(ready).ToArray());
        WriteAtomic(_resultsPath, JsonSerializer.Serialize(batch, Json));
        _published.AddRange(ready);
        _staged.RemoveRange(0, ready.Length);
        if (_published.Count != 0) _publishedThrough = _published[^1].Sequence;
        _mailboxDirty = false;
    }

    public bool Acknowledge(BlockReceiptAck ack)
    {
        if (ack.Version != 1 || ack.Session != Session || ack.Sequence < _ack || ack.Sequence > _publishedThrough) return false;
        if (ack.Sequence == _ack) return true;
        _ack = ack.Sequence;
        _published.RemoveAll(r => r.Sequence <= _ack);
        _mailboxDirty = true;
        return true;
    }

    public void ReadAcknowledgement()
    {
        try
        {
            var info = new FileInfo(_ackPath);
            if (!info.Exists || info.Length > 4096) return;
            var ack = JsonSerializer.Deserialize<BlockReceiptAck>(File.ReadAllText(_ackPath), Json);
            if (ack is not null) Acknowledge(ack);
        }
        catch (IOException) { }
        catch (JsonException) { }
        catch (UnauthorizedAccessException) { }
    }

    private static void WriteAtomic(string path, string payload)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var temporary = path + ".tmp";
        using (var file = new FileStream(temporary, FileMode.Create, FileAccess.Write, FileShare.None))
        {
            var bytes = System.Text.Encoding.UTF8.GetBytes(payload);
            file.Write(bytes);
            file.Flush(flushToDisk: true);
        }
        File.Move(temporary, path, overwrite: true);
    }
}
