using System.Diagnostics;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class TimeApi : IHostTimeApi
    {
        private readonly Stopwatch _clock;
        private readonly Func<ulong> _tickId;

        public TimeApi(Stopwatch clock, Func<ulong> tickId)
        {
            _clock = clock;
            _tickId = tickId;
        }

        public double NowSeconds => _clock.Elapsed.TotalSeconds;

        public ulong TickId => _tickId();

        public double TickRate => 60.0;
    }
}
