using System.Diagnostics;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

internal sealed partial class RemoteTransportClient
{
    private readonly bool _traceTiming = Environment.GetEnvironmentVariable("OCTARYN_REMOTE_TIMING") == "1";
    private readonly SortedDictionary<ulong, long> _timingEdges = new();
    private bool? _timingJump;
    private bool? _timingGrounded;
    private ulong _timingLastFrame;

    private void ResetTiming()
    {
        _timingEdges.Clear();
        _timingJump = _timingGrounded = null;
        _timingLastFrame = 0;
        _lastCommandSend = 0;
    }

    private void TraceIntent(RemoteIntentKind kind, byte[] payload)
    {
        if (!_traceTiming || kind != RemoteIntentKind.PlayerInput) return;
        var now = Stopwatch.GetTimestamp();
        foreach (var command in PlayerCommandPacket.ReadJson(payload))
        {
            if (command.FrameIndex <= _timingLastFrame) continue;
            _timingLastFrame = command.FrameIndex;
            var jump = (command.Flags & 1u) != 0;
            if (jump == _timingJump) continue;
            _timingJump = jump;
            if (_timingEdges.Count < 256) _timingEdges[command.FrameIndex] = now;
            Console.Error.WriteLine($"remote_timing event=input timestamp={now} frame={command.FrameIndex} jump={(jump ? 1 : 0)}");
        }
    }

    private void TracePose(in SessionPose pose)
    {
        if (!_traceTiming) return;
        var elapsed = -1.0;
        foreach (var edge in _timingEdges.ToArray())
        {
            if (edge.Key > pose.AcknowledgedInputFrame) break;
            elapsed = Stopwatch.GetElapsedTime(edge.Value).TotalMilliseconds;
            Console.Error.WriteLine(FormattableString.Invariant(
                $"remote_timing event=input_ack timestamp={Stopwatch.GetTimestamp()} frame={edge.Key} ack={pose.AcknowledgedInputFrame} input_to_ack_ms={elapsed:F3}"));
            _timingEdges.Remove(edge.Key);
        }
        if (elapsed < 0 && pose.OnGround == _timingGrounded) return;
        var manager = _entityManager;
        Console.Error.WriteLine(FormattableString.Invariant(
            $"remote_timing event=pose timestamp={Stopwatch.GetTimestamp()} tick={pose.SourceTick} ack={pose.AcknowledgedInputFrame} input_to_ack_ms={elapsed:F3} grounded={(pose.OnGround ? 1 : 0)} y={pose.Y:F4} vy={pose.VelocityY:F4} les_states={manager?.LerpBufferCount ?? 0} les_buffer_ms={(manager?.LerpBufferTimeLength ?? 0) * 1000:F3} les_jitter_ms={(manager?.NetworkJitter ?? 0) * 1000:F3}"));
        _timingGrounded = pose.OnGround;
    }
}
