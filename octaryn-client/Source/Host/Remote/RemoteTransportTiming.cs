using System.Diagnostics;
using System.Text.Json;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

internal sealed partial class RemoteTransportClient
{
    private readonly bool _traceTiming = Environment.GetEnvironmentVariable("OCTARYN_REMOTE_TIMING") == "1";
    private bool? _timingJump;
    private bool? _timingGrounded;
    private ulong? _timingPendingFrame;
    private long _timingSentAt;

    private void TraceIntent(RemoteIntentKind kind, byte[] payload)
    {
        if (!_traceTiming || kind != RemoteIntentKind.PlayerInput)
            return;
        try
        {
            using var json = JsonDocument.Parse(payload);
            var root = json.RootElement;
            if (!root.TryGetProperty("flags", out var flags) ||
                !root.TryGetProperty("frameIndex", out var frame))
                return;
            var jump = (flags.GetUInt32() & 1u) != 0;
            if (jump == _timingJump)
                return;
            _timingJump = jump;
            _timingPendingFrame = frame.GetUInt64();
            _timingSentAt = Stopwatch.GetTimestamp();
            Console.Error.WriteLine($"remote_timing event=input timestamp={_timingSentAt} frame={_timingPendingFrame} jump={(jump ? 1 : 0)}");
        }
        catch (Exception ex) when (ex is JsonException or InvalidOperationException or FormatException or OverflowException)
        {
            // Diagnostics do not reject or alter forwarded input.
        }
    }

    private void TracePose(in SessionPose pose)
    {
        if (!_traceTiming)
            return;
        var acknowledged = _timingPendingFrame.HasValue && pose.AcknowledgedInputFrame >= _timingPendingFrame.Value;
        if (!acknowledged && pose.OnGround == _timingGrounded)
            return;
        var elapsed = acknowledged ? Stopwatch.GetElapsedTime(_timingSentAt).TotalMilliseconds : -1;
        var manager = _entityManager;
        Console.Error.WriteLine(FormattableString.Invariant(
            $"remote_timing event=pose timestamp={Stopwatch.GetTimestamp()} tick={pose.SourceTick} ack={pose.AcknowledgedInputFrame} input_to_ack_ms={elapsed:F3} grounded={(pose.OnGround ? 1 : 0)} y={pose.Y:F4} vy={pose.VelocityY:F4} les_states={manager?.LerpBufferCount ?? 0} les_buffer_ms={(manager?.LerpBufferTimeLength ?? 0) * 1000:F3} les_jitter_ms={(manager?.NetworkJitter ?? 0) * 1000:F3}"));
        _timingGrounded = pose.OnGround;
        if (acknowledged)
            _timingPendingFrame = null;
    }
}
