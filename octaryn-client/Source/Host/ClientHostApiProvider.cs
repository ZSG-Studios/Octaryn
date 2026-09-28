using System.Diagnostics;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Client.Host;

// Managed host API backend for the in-process client path when the native
// host did not vend API tables through the query function.
internal sealed class ClientHostApiProvider : IHostApiProvider
{
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    private readonly NativeScheduleRuntime _scheduleRuntime;
    private HostInputSnapshot? _latestInput;

    public ClientHostApiProvider(NativeScheduleRuntime scheduleRuntime)
    {
        _scheduleRuntime = scheduleRuntime;
    }

    // Called by the module activator on every host tick.
    public void SetLatestInput(HostInputSnapshot input)
    {
        _latestInput = input;
    }

    public IHostTimeApi GetTimeApi()
    {
        return new TimeApi(_clock);
    }

    public IHostDiagnosticsApi GetDiagnosticsApi()
    {
        return new DiagnosticsApi();
    }

    // Client collision and map facts are render-side only today; the physics
    // and world tables arrive with the client prediction mirror.
    public IHostPhysicsApi? GetPhysicsApi() => null;

    public IHostWorldApi? GetWorldApi() => null;

    // Player authority always lives on the server host.
    public IHostPlayerApi? GetPlayerApi() => null;

    // Client module ECS arrives with the client host bridge; unavailable today.
    public IHostEcsApi? GetEcsApi() => null;

    public IHostInputApi GetInputApi()
    {
        return new InputApi(this);
    }

    public IHostSchedulingApi GetSchedulingApi()
    {
        return new SchedulingApi(_scheduleRuntime);
    }

    // Audio and UI execution live in the native client app (miniaudio/RmlUi);
    // they stay unavailable until the managed-to-native presentation channel
    // exists. Modules see absence, never silent drops.
    public IHostAudioApi? GetAudioApi() => null;

    public IHostUiApi? GetUiApi() => null;

    public IHostReplicationApi? GetReplicationApi() => null;

    private sealed class TimeApi : IHostTimeApi
    {
        private readonly Stopwatch _clock;

        public TimeApi(Stopwatch clock)
        {
            _clock = clock;
        }

        public double NowSeconds => _clock.Elapsed.TotalSeconds;

        public ulong TickId => 0;

        public double TickRate => 60.0;
    }

    private sealed class DiagnosticsApi : IHostDiagnosticsApi
    {
        public void Write(HostLogLevel level, string message)
        {
            Debug.WriteLine($"[module_api:{level}] {message}");
        }
    }

    private sealed class InputApi : IHostInputApi
    {
        private readonly ClientHostApiProvider _owner;

        public InputApi(ClientHostApiProvider owner)
        {
            _owner = owner;
        }

        public bool TryPollInput(out HostInputState input)
        {
            if (_owner._latestInput is not { } snapshot)
            {
                input = default;
                return false;
            }

            input = new HostInputState(
                snapshot.Flags,
                snapshot.Controller,
                snapshot.MoveX,
                snapshot.MoveY,
                snapshot.MoveZ,
                snapshot.CameraPitch,
                snapshot.CameraYaw);
            return true;
        }
    }

    private sealed class SchedulingApi : IHostSchedulingApi
    {
        private readonly NativeScheduleRuntime _runtime;

        public SchedulingApi(NativeScheduleRuntime runtime)
        {
            _runtime = runtime;
        }

        public void RunOnMainThread(string jobId, Action work)
        {
            _runtime.ExecuteCommandWriteMainThread(jobId, work);
        }

        public void RunOnWorker(string jobId, Action work)
        {
            _runtime.ExecuteWorker(jobId, work);
        }
    }
}
