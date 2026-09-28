using System.Diagnostics;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.MapWorld;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Host;

// Managed host API backend for the production server path, which activates
// modules without a native host bridge. Native-bridged hosts can substitute
// NativeHostApiProvider instead.
internal sealed class ServerHostApiProvider : IHostApiProvider, IDisposable
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

    private sealed class DiagnosticsApi : IHostDiagnosticsApi
    {
        public void Write(HostLogLevel level, string message)
        {
            LiveDebugLog.Write($"module_api level={level} {message}");
        }
    }

    private sealed class PlayerApi : IHostPlayerApi
    {
        private readonly PlayerController _player;

        public PlayerApi(PlayerController player)
        {
            _player = player;
        }

        public bool TryGetState(out HostCharacterState state)
        {
            var snapshot = _player.Snapshot();
            state = new HostCharacterState
            {
                X = snapshot.X,
                Y = snapshot.Y,
                Z = snapshot.Z,
                Pitch = snapshot.Pitch,
                Yaw = snapshot.Yaw,
                VelocityX = snapshot.VelocityX,
                VelocityY = snapshot.VelocityY,
                VelocityZ = snapshot.VelocityZ,
                IsOnGround = snapshot.IsOnGround,
                ControlMode = snapshot.ControlMode,
                JumpHeld = snapshot.JumpHeld,
            };
            return true;
        }

        public void SetState(in HostCharacterState state)
        {
            _player.SetState(new PlayerState(
                state.X,
                state.Y,
                state.Z,
                state.Pitch,
                state.Yaw,
                state.VelocityX,
                state.VelocityY,
                state.VelocityZ,
                state.IsOnGround,
                state.ControlMode,
                state.JumpHeld));
        }
    }

    private sealed class PhysicsApi : IHostPhysicsApi
    {
        private readonly IntPtr _mapWorld;

        public PhysicsApi(IntPtr mapWorld)
        {
            _mapWorld = mapWorld;
        }

        public bool Raycast(
            float originX, float originY, float originZ,
            float directionX, float directionY, float directionZ,
            float maxDistance, out HostRaycastHit hit)
        {
            return NativeMapWorld.Raycast(
                _mapWorld,
                originX, originY, originZ,
                directionX, directionY, directionZ,
                maxDistance, out hit) == 0;
        }

        public unsafe bool MoveCharacter(in HostCharacterInput input, double deltaSeconds, ref HostCharacterState state)
        {
            var nativeInput = new NativeInput(
                input.Flags,
                input.Controller,
                input.MoveX,
                input.MoveY,
                input.MoveZ,
                input.CameraX,
                input.CameraY,
                input.CameraZ,
                input.CameraPitch,
                input.CameraYaw,
                input.RelativeMouse);
            var nativeState = new NativeState(
                state.X, state.Y, state.Z,
                state.Pitch, state.Yaw,
                state.VelocityX, state.VelocityY, state.VelocityZ,
                state.IsOnGround ? 1u : 0u,
                state.ControlMode,
                (ushort)(state.JumpHeld ? 1 : 0));
            var tickResult = default(NativeTickResult);

            var result = NativeMapWorld.StepInto(
                _mapWorld, &nativeInput, deltaSeconds, &nativeState, &tickResult);
            if (result != 0)
            {
                return false;
            }

            state.X = nativeState.X;
            state.Y = nativeState.Y;
            state.Z = nativeState.Z;
            state.Pitch = nativeState.Pitch;
            state.Yaw = nativeState.Yaw;
            state.VelocityX = nativeState.VelocityX;
            state.VelocityY = nativeState.VelocityY;
            state.VelocityZ = nativeState.VelocityZ;
            state.IsOnGround = nativeState.IsOnGround != 0;
            state.ControlMode = nativeState.ControlMode;
            state.JumpHeld = nativeState.JumpHeld != 0;
            return true;
        }

        public bool StepWorldItem(ref HostWorldItemState state, double deltaSeconds)
        {
            var nativeState = new HostWorldItemStateNative
            {
                X = state.X,
                Y = state.Y,
                Z = state.Z,
                VelocityX = state.VelocityX,
                VelocityY = state.VelocityY,
                VelocityZ = state.VelocityZ,
                Grounded = state.Grounded ? 1u : 0u,
                Sleeping = state.Sleeping ? 1u : 0u,
                SleepTimer = state.SleepTimer
            };

            if (NativeMapWorld.StepItem(_mapWorld, ref nativeState, deltaSeconds) != 0)
            {
                return false;
            }

            state.X = nativeState.X;
            state.Y = nativeState.Y;
            state.Z = nativeState.Z;
            state.VelocityX = nativeState.VelocityX;
            state.VelocityY = nativeState.VelocityY;
            state.VelocityZ = nativeState.VelocityZ;
            state.Grounded = nativeState.Grounded != 0;
            state.Sleeping = nativeState.Sleeping != 0;
            state.SleepTimer = nativeState.SleepTimer;
            return true;
        }
    }

    private sealed class WorldApi : IHostWorldApi
    {
        private readonly IntPtr _mapWorld;

        public WorldApi(IntPtr mapWorld)
        {
            _mapWorld = mapWorld;
        }

        public bool IsActive => true;

        public ulong TriangleCount => NativeMapWorld.TriangleCount(_mapWorld);

        public bool TryGetSpawnPose(out HostSpawnPose pose)
        {
            var state = NativeMapWorld.SpawnState(_mapWorld);
            pose = new HostSpawnPose(state.X, state.Y, state.Z, state.Yaw, state.Pitch);
            return true;
        }
    }

    private sealed class InputApi : IHostInputApi
    {
        private readonly ServerHostApiProvider _owner;

        public InputApi(ServerHostApiProvider owner)
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

            // Mirror the native has_input_intent contract: a neutral snapshot is
            // telemetry, not a look — the character keeps its current angles.
            if (snapshot is { Controller: 0, Flags: 0, MoveX: 0.0f, MoveY: 0.0f, MoveZ: 0.0f, RelativeMouse: 0 })
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

    private sealed class UiApi : IHostUiApi
    {
        private readonly ServerHostApiProvider _owner;

        public UiApi(ServerHostApiProvider owner)
        {
            _owner = owner;
        }

        // The authority has no notification surface; notifications travel with
        // the replication channel instead. Report unhandled, never fake-shown.
        public bool ShowNotification(string text)
        {
            return false;
        }

        public bool TryPollAction(out string actionId)
        {
            if (_owner._uiActions.Count == 0 || (_owner._replicationChannel?.AvailableChangeCapacity ?? int.MaxValue) < 2)
            {
                actionId = string.Empty;
                return false;
            }

            actionId = _owner._uiActions.Dequeue();
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

    private sealed class ReplicationApi : IHostReplicationApi
    {
        private readonly ServerHostApiProvider _owner;
        public int AvailableChangeCapacity => _owner._replicationChannel?.AvailableChangeCapacity ?? int.MaxValue;
        public int AvailableWorldItemCapacity => _owner.WorldItems.Available;
        public bool PublishWorldItem(in HostWorldItemPose pose) => _owner.WorldItems.Publish(pose, _owner._tickId());

        public ReplicationApi(ServerHostApiProvider owner)
        {
            _owner = owner;
        }

        public bool PublishChange(uint changeKind, ulong replicationId, ulong payload0, ulong payload1)
        {
            var data = new ModuleEventData
            {
                EventId = replicationId,
                Payload0 = changeKind,
                Payload1 = payload0,
                Payload2 = payload1
            };
            return _owner._replicationChannel?.Broadcast(in data) ?? false;
        }

        // The LES session channel carries fixed 32-byte ModuleEventData only;
        // variable byte payloads need the native mailbox table.
        public int SendMessage(ulong replicationId, ReadOnlySpan<byte> payload)
        {
            _ = replicationId;
            _ = payload;
            return -2;
        }
    }
}
