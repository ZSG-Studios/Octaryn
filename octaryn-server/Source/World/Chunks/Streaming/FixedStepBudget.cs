using System.Diagnostics;

namespace Octaryn.Server;

internal sealed class FixedStepBudget
{
    internal const double Delta = 1.0 / 60.0;
    internal const int MaximumSteps = 8;
    private readonly Func<double> _clock;
    private double _previous;
    private double _credit;

    internal FixedStepBudget(Func<double>? clock = null)
    {
        _clock = clock ?? (() => Stopwatch.GetTimestamp() / (double)Stopwatch.Frequency);
        Reset();
    }

    internal bool CanStep => _credit + 1e-10 >= Delta;

    internal void Reset()
    {
        _previous = _clock();
        _credit = 0;
    }

    internal void Accrue()
    {
        var now = _clock();
        _credit = Math.Min(MaximumSteps * Delta, _credit + Math.Max(0, now - _previous));
        _previous = now;
    }

    internal void Commit() => _credit = Math.Max(0, _credit - Delta);
}
