namespace Octaryn.Server;

// One latest-state publication per deadline, with no catch-up burst or phase drift.
internal sealed class PlayerStatePublicationClock
{
    internal const double IntervalSeconds = 1.0 / 60.0;
    private bool _published;
    private ulong _lastTick;
    private double _nextDeadline;

    internal bool ShouldPublish(double now, ulong sourceTick) =>
        double.IsFinite(now) && now >= 0 &&
        (!_published || (sourceTick > _lastTick && now >= _nextDeadline));

    internal void Published(double now, ulong sourceTick)
    {
        if (!ShouldPublish(now, sourceTick))
            throw new InvalidOperationException("Player publication acknowledgement has no due state.");
        _nextDeadline = !_published ? now + IntervalSeconds
            : _nextDeadline + (Math.Floor((now - _nextDeadline) / IntervalSeconds) + 1) * IntervalSeconds;
        _lastTick = sourceTick;
        _published = true;
    }

    // A new session must not inherit the previous session's deadline/tick watermark.
    internal void Reset()
    {
        _published = false;
        _lastTick = 0;
        _nextDeadline = 0;
    }
}
