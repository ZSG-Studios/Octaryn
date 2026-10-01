using System.Diagnostics;
using Octaryn.Server.Simulation.Players;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Host;

// Managed host API backend for the production server path, which activates
// modules without a native host bridge. Native-bridged hosts can substitute
// NativeHostApiProvider instead. The per-domain API implementations live in
// the ServerHostApiProvider.*.cs partial files.
internal sealed partial class ServerHostApiProvider : IHostApiProvider, IDisposable
{
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    private readonly Func<ulong> _tickId;
    private readonly IntPtr _mapWorld;
    private readonly NativeScheduleRuntime _scheduleRuntime;
    private readonly PlayerController _playerController;
    private ArchHostEcsApi? _ecs;
    private HostInputSnapshot? _latestInput;
    private readonly Queue<string> _uiActions = new();
    private Networking.Remote.IServerReplicationChannel? _replicationChannel;
    private readonly LocalModuleEventMailbox? _localEvents;
    internal readonly Networking.Remote.WorldItemRegistry WorldItems = new();
    private ulong _acknowledgedUiActions, _acknowledgedUiActionEpoch;

    public ServerHostApiProvider(
        Func<ulong> tickId,
        IntPtr mapWorld,
        NativeScheduleRuntime scheduleRuntime,
        PlayerController playerController)
    {
        _tickId = tickId;
        _mapWorld = mapWorld;
        _scheduleRuntime = scheduleRuntime;
        _playerController = playerController;
        var eventsPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_MODULE_EVENTS_PATH");
        if (!string.IsNullOrWhiteSpace(eventsPath))
        {
            _localEvents = new LocalModuleEventMailbox(eventsPath);
            _replicationChannel = _localEvents;
        }
    }

    public void Dispose()
    {
        _ecs?.Dispose();
        _ecs = null;
        _uiActions.Clear();
        _replicationChannel = null;
    }

    // Called by the module activator on every host tick.
    public void SetLatestInput(HostInputSnapshot input)
    {
        _latestInput = input;
        _localEvents?.Flush();
    }

    // Attached by RemoteServer when a dedicated session starts.
    public void SetReplicationChannel(Networking.Remote.IServerReplicationChannel channel)
    {
        _replicationChannel = channel;
    }

    public bool AcknowledgeUiActions(ulong epoch, ulong sequence)
    {
        if (_localEvents is not null || (epoch == _acknowledgedUiActionEpoch && sequence == _acknowledgedUiActions)) return true;
        var data = new ModuleEventData { EventId = sequence, Payload0 = RemoteProtocol.UiActionAckEventKind, Payload1 = epoch };
        if (_replicationChannel?.Broadcast(in data) == true)
        {
            _acknowledgedUiActions = sequence;
            _acknowledgedUiActionEpoch = epoch;
            return true;
        }
        return false;
    }

    public IHostTimeApi GetTimeApi()
    {
        return new TimeApi(_clock, _tickId);
    }

    public IHostDiagnosticsApi GetDiagnosticsApi()
    {
        return new DiagnosticsApi();
    }

    public IHostPhysicsApi? GetPhysicsApi()
    {
        return _mapWorld != IntPtr.Zero ? new PhysicsApi(_mapWorld) : null;
    }

    public IHostWorldApi? GetWorldApi()
    {
        return _mapWorld != IntPtr.Zero ? new WorldApi(_mapWorld) : null;
    }

    // Player authority storage always exists on the server; the module decides
    // whether it drives the state or leaves the native step in charge.
    public IHostPlayerApi GetPlayerApi()
    {
        return new PlayerApi(_playerController);
    }

    // One ECS backend per module activation; Arch never crosses the boundary.
    public IHostEcsApi GetEcsApi()
    {
        _ecs ??= new ArchHostEcsApi();
        return _ecs;
    }

    public IHostInputApi GetInputApi()
    {
        return new InputApi(this);
    }

    public IHostSchedulingApi GetSchedulingApi()
    {
        return new SchedulingApi(_scheduleRuntime);
    }

    // Audio and UI are client presentation backends; the authority never
    // renders or mixes, so the server vends nothing for them.
    public IHostAudioApi? GetAudioApi() => null;

    // Server-side queue for module-visible UI actions; filled from the session
    // ui_action intent file each tick.
    public IHostUiApi GetUiApi()
    {
        return new UiApi(this);
    }

    public void ResetUiActions()
    {
        _uiActions.Clear();
        _acknowledgedUiActions = 0;
        _acknowledgedUiActionEpoch = 0;
    }

    public bool EnqueueUiAction(string actionId)
    {
        if (_replicationChannel?.AvailableChangeCapacity == 0) return false;
        if (_uiActions.Count >= 256 || string.IsNullOrWhiteSpace(actionId)) return false;
        _uiActions.Enqueue(actionId);
        return true;
    }

    // Local sessions retain events in an acknowledged mailbox; dedicated
    // sessions attach their ordered transport channel.
    public IHostReplicationApi? GetReplicationApi()
    {
        return new ReplicationApi(this);
    }
}
