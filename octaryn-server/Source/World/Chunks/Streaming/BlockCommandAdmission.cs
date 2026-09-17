using System.Diagnostics;
using System.Text.Json;
using Octaryn.Server.Modules;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal sealed class BlockCommandAdmission
{
    internal enum Decision { Wait, Submit, Resolved }
    private ulong _frame;
    private long _received;
    private int _nextCommand;
    private int _commandCount;
    private HostCommand[] _reserved = [];
    internal bool IsComplete => _nextCommand == _commandCount && _reserved.Length == 0;

    internal void Reset()
    {
        _frame = 0;
        _received = 0;
        _nextCommand = _commandCount = 0;
        _reserved = [];
    }

    internal Decision Prepare(ModuleActivator owner, string path, ulong frame,
        ReadOnlySpan<HostCommand> commands, out HostCommand[] admitted)
    {
        admitted = [];
        if (_frame != frame)
        {
            foreach (var abandoned in _reserved) owner.RejectAdmittedBlockInteraction(in abandoned);
            _frame = frame;
            _received = Stopwatch.GetTimestamp();
            _nextCommand = 0;
            _commandCount = commands.Length;
            _reserved = [];
        }
        if (_reserved.Length != 0)
        {
            admitted = _reserved;
            return Decision.Submit;
        }
        if (!TryReadMovementDependency(path, frame, out var movementFrame)) return Decision.Wait;
        var dependency = ChunkStreamProcessBridge.EvaluateCommandDependency(movementFrame,
            Stopwatch.GetElapsedTime(_received).TotalSeconds);
        if (dependency == ChunkStreamProcessBridge.CommandDependency.Wait) return Decision.Wait;

        var reserved = new List<HostCommand>();
        for (; _nextCommand < commands.Length; _nextCommand++)
        {
            var command = commands[_nextCommand];
            var admission = owner.AdmitBlockInteraction(in command,
                dependency == ChunkStreamProcessBridge.CommandDependency.Reject ? 0 : movementFrame,
                ChunkStreamProcessBridge.ConsumedPlayerCommand, Environment.TickCount64);
            if (admission == BlockInteractionAdmission.Deferred) break;
            if (admission != BlockInteractionAdmission.Ready) continue;
            if (dependency == ChunkStreamProcessBridge.CommandDependency.Reject)
                owner.RejectAdmittedBlockInteraction(in command);
            else reserved.Add(command);
        }
        _reserved = admitted = reserved.ToArray();
        if (_reserved.Length != 0) return Decision.Submit;
        return IsComplete ? Decision.Resolved : Decision.Wait;
    }

    internal void Submitted() => _reserved = [];

    private static bool TryReadMovementDependency(string path, ulong frame, out ulong movementFrame)
    {
        movementFrame = 0;
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            if (!root.TryGetProperty("frameIndex", out var index) || !index.TryGetUInt64(out var current) ||
                current != frame) return false;
            return !root.TryGetProperty("movementFrameID", out var movement) || movement.TryGetUInt64(out movementFrame);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException or InvalidOperationException)
        {
            return false;
        }
    }
}
