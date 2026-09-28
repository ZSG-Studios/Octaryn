using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Time;

// Advances the world clock component from the authoritative frame time and
// emits sparse hour transitions for diagnostics.
public sealed class WorldTimeSystem
{
    private readonly IHostDiagnosticsApi? _diagnostics;

    public WorldTimeSystem(IHostDiagnosticsApi? diagnostics)
    {
        _diagnostics = diagnostics;
    }

    public void Tick(IHostEcsApi ecs, in ModuleFrameContext frame)
    {
        var worldTime = frame.WorldTime;
        ecs.Query<WorldClockComponent>((ModuleEntity _, ref WorldClockComponent clock) =>
        {
            clock.TickId = worldTime.TickId;
            clock.TotalSeconds = worldTime.TotalSeconds;
            var hour = (int)System.Math.Floor(clock.DayFraction01 * 24.0);
            if (hour != clock.LastHour)
            {
                clock.LastHour = hour;
                _diagnostics?.Write(
                    HostLogLevel.Debug,
                    $"octaryn.basegame world_time hour={hour} day_fraction={clock.DayFraction01:F4}");
            }
        });
    }
}
