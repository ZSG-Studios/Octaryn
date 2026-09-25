using System.Diagnostics;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server;

internal sealed class PlayerCommandQueue
{
    internal const double FixedDelta = FixedStepBudget.Delta;
    private readonly Queue<(ulong Sequence, HostInputSnapshot Input, double Received)> _pending = new();
    private readonly Func<double> _clock;
    private readonly FixedStepBudget _budget;
    private ulong _accepted;
    private ulong _retiredSequence;
    private double _lastArrival = double.NegativeInfinity;
    private HostInputSnapshot _held = Neutral(default);
    private readonly bool _traceTiming = Environment.GetEnvironmentVariable("OCTARYN_REMOTE_TIMING") == "1";
    private double _lastTimingLog;
    private ulong _highestReceived;
    private double Now => _clock();
    internal ulong Acknowledged { get; private set; }
    internal ulong SelectedSequence { get; private set; }

    internal PlayerCommandQueue(Func<double>? clock = null)
    {
        _clock = clock ?? (() => Stopwatch.GetTimestamp() / (double)Stopwatch.Frequency);
        _budget = new FixedStepBudget(_clock);
        Reset();
    }

    internal void Reset()
    {
        _pending.Clear();
        _accepted = Acknowledged = SelectedSequence = _retiredSequence = 0;
        _lastArrival = double.NegativeInfinity;
        _budget.Reset();
        _held = Neutral(default);
        _lastTimingLog = 0;
        _highestReceived = 0;
    }

    internal void Read(string? path)
    {
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path)) return;
        try
        {
            if (new FileInfo(path).Length > 131072) return;
            Accept(PlayerCommandPacket.ReadJson(File.ReadAllBytes(path)));
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        { }
    }

    internal void Accept(ReadOnlySpan<PlayerCommand> commands)
    {
        if (commands.Length > PlayerCommandPacket.MaxBatch) return;
        ulong previous = 0;
        foreach (var command in commands)
        {
            if (!PlayerCommandPacket.Valid(in command) || (previous != 0 && command.FrameIndex != previous + 1)) return;
            previous = command.FrameIndex;
        }
        var now = Now;
        var before = _accepted;
        if (commands.Length != 0) _highestReceived = Math.Max(_highestReceived, commands[^1].FrameIndex);
        foreach (var command in commands)
        {
            if (command.FrameIndex <= _accepted) continue;
            if (command.FrameIndex != _accepted + 1 || _pending.Count >= 256 || command.FrameIndex - Acknowledged > 256) break;
            var input = new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue,
                command.Flags, command.Controller, command.MoveX, command.MoveY, command.MoveZ,
                0, 0, 0, command.CameraPitch, command.CameraYaw, command.RelativeMouse);
            _pending.Enqueue((command.FrameIndex, input, now));
            _accepted = command.FrameIndex;
            _lastArrival = now;
        }
        if (_accepted != before)
            LiveDebugLog.Write($"server_live_player_input_intent active=1 source=command_batch version=2 first={before + 1} last={_accepted} pending={_pending.Count}");
        if (_traceTiming && now - _lastTimingLog >= 0.25)
        {
            _lastTimingLog = now;
            var oldestAge = _pending.TryPeek(out var oldest) ? (now - oldest.Received) * 1000 : 0;
            Console.Error.WriteLine(FormattableString.Invariant(
                $"server_command_timing received={_highestReceived} accepted={_accepted} ack={Acknowledged} pending={_pending.Count} oldest_ms={oldestAge:F1}"));
        }
    }

    internal void Accrue() => _budget.Accrue();

    internal void SeedIdleView(float pitch, float yaw, uint controlMode)
    {
        if (_accepted != 0) return;
        _held = new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue,
            controlMode == 1 ? HostInputSnapshot.FlyModeFlag : 0, 1,
            0, 0, 0, 0, 0, 0, pitch, yaw, 1);
    }

    internal bool TrySelect(ulong tick, out HostFrameSnapshot frame)
    {
        frame = default;
        if (!_budget.CanStep) return false;
        var now = Now;
        SelectedSequence = 0;
        while (_pending.TryPeek(out var expired) && now - expired.Received > 0.25)
        {
            _retiredSequence = expired.Sequence;
            _pending.Dequeue();
        }
        // Transport delay must not cause held input to be simulated twice.
        if (_pending.Count == 0 && now - _lastArrival <= 0.25 && _retiredSequence == 0) return false;
        var input = Neutral(_held);
        if (_pending.TryPeek(out var command))
        {
            input = command.Input;
            SelectedSequence = command.Sequence;
        }
        frame = new HostFrameSnapshot(input, new HostFrameTimingSnapshot(
            HostFrameTimingSnapshot.VersionValue, HostFrameTimingSnapshot.SizeValue, tick, FixedDelta));
        return true;
    }

    internal void Commit(in HostFrameSnapshot frame)
    {
        _budget.Commit();
        _held = frame.Input;
        if (_retiredSequence != 0)
        {
            if (_traceTiming)
                Console.Error.WriteLine($"server_command_expired first={Acknowledged + 1} through={_retiredSequence}");
            Acknowledged = _retiredSequence;
            _retiredSequence = 0;
        }
        if (SelectedSequence == 0) return;
        _pending.Dequeue();
        Acknowledged = SelectedSequence;
    }

    private static HostInputSnapshot Neutral(HostInputSnapshot input) => new(
        HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue,
        input.Flags & HostInputSnapshot.FlyModeFlag, 1, 0, 0, 0, 0, 0, 0,
        input.CameraPitch, input.CameraYaw, 1);
}
