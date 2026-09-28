using Octaryn.Server.Persistence.World;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Simulation.Players;

internal sealed class PlayerController : IDisposable
{
    private readonly PlayerSimulationWorld _simulation;
    private readonly PlayerSimulationIdentity _identity;
    private readonly Func<bool> _collisionReady;
    private readonly Action<HostFrameContext> _consumeCommand;
    private readonly PlayerSaveQueue _saves;
    private ulong _completedSave;
    private bool _disposed;

    public PlayerController(
        string playerDirectory,
        PlayerSimulationWorld simulation,
        int playerId = 1)
    {
        _simulation = simulation;
        _identity = _simulation.Add(playerId,
            LoadInitialState(playerDirectory, playerId, out var loadedFromSave),
            loadedFromSave);
        _collisionReady = () => _simulation.CollisionReady(_identity);
        _consumeCommand = command => TickCommand(in command);
        _saves = new PlayerSaveQueue(playerDirectory, playerId);
        var state = _simulation.Snapshot(_identity);
        LiveDebugLog.Write(
            $"server_live_player_load loaded={(loadedFromSave ? 1 : 0)} " +
            $"pos=({state.X:F3},{state.Y:F3},{state.Z:F3}) " +
            $"pitch={state.Pitch:F6} yaw={state.Yaw:F6}");
    }

    public PlayerState Snapshot()
    {
        ThrowIfDisposed();
        return _simulation.Snapshot(_identity);
    }

    // Module-driven authority: writes the module-computed state. Snapshots and
    // persistence keep flowing through the same simulation storage.
    public void SetState(PlayerState state)
    {
        ThrowIfDisposed();
        _simulation.SetState(_identity, state);
    }

    // When set (module player authority), each consumed command routes to this
    // step instead of the native simulation step. Persistence still runs here.
    public Action<HostFrameContext>? StepOverride { get; set; }

    // Existing saves win over the manifest's new-player spawn.
    public void ApplyMapSpawn()
    {
        ThrowIfDisposed();
        var loadedFromSave = _simulation.LoadedFromSave(_identity);
        var diagnosticSpawn = Environment.GetEnvironmentVariable("OCTARYN_SERVER_DIAGNOSTIC_MAP_SPAWN") == "1";
        if (loadedFromSave && !diagnosticSpawn)
        {
            LiveDebugLog.Write("server_live_player_spawn_align active=0 source=saved_pose loaded=1");
            return;
        }
        if (!_simulation.AlignSpawnWithMap(_identity, out var spawned))
        {
            LiveDebugLog.Write(
                $"server_live_player_spawn_align active=0 source=map_manifest " +
                $"loaded={(loadedFromSave ? 1 : 0)}");
            return;
        }

        var persisted = SaveIfDue(0.0, force: true);
        LiveDebugLog.Write(
            $"server_live_player_spawn_align active=1 source=map_manifest " +
            $"loaded={(loadedFromSave ? 1 : 0)} diagnostic={(diagnosticSpawn ? 1 : 0)} " +
            $"pos=({spawned.X:F3},{spawned.Y:F3},{spawned.Z:F3}) " +
            $"pitch={spawned.Pitch:F6} yaw={spawned.Yaw:F6} saved={(persisted ? 1 : 0)}");
    }

    public void Tick(in HostFrameContext frame)
    {
        if (ChunkStreamProcessBridge.CommandAuthorityActive)
        {
            ChunkStreamProcessBridge.ConsumePlayerCommands(Snapshot(),
                _collisionReady, _consumeCommand);
            return;
        }
        if (_simulation.CollisionReady(_identity, frame.DeltaSeconds)) TickCommand(in frame);
    }

    private void TickCommand(in HostFrameContext frame)
    {
        var input = frame.Input;
        ThrowIfDisposed();
        // Only real consumed commands reach the module step. Placeholder world
        // frames (no selected command, controller zero) carry no camera intent
        // and would zero the module-held view angles.
        if (StepOverride is { } moduleStep && input.Controller != 0)
        {
            moduleStep(frame);
            var moduleState = _simulation.Snapshot(_identity);
            var modulePersisted = SaveIfDue(frame.DeltaSeconds);
            LiveDebugLog.Write(
                $"server_live_player_state frame={frame.FrameIndex} tick_input=1 authority=module " +
                $"mode={NativePlayerSimulation.ControlModeName(moduleState.ControlMode)} flags={input.Flags} controller={input.Controller} " +
                $"move=({input.MoveX:F3},{input.MoveY:F3},{input.MoveZ:F3}) " +
                $"pos=({moduleState.X:F3},{moduleState.Y:F3},{moduleState.Z:F3}) " +
                $"pitch={moduleState.Pitch:F6} yaw={moduleState.Yaw:F6} " +
                $"velocity=({moduleState.VelocityX:F3},{moduleState.VelocityY:F3},{moduleState.VelocityZ:F3}) " +
                $"ground={(moduleState.IsOnGround ? 1 : 0)} saved={(modulePersisted ? 1 : 0)}");
            return;
        }

        var state = _simulation.StepOne(_identity, frame, out var tickResult);
        var persisted = SaveIfDue(frame.DeltaSeconds);
        LiveDebugLog.Write(
            $"server_live_player_state frame={frame.FrameIndex} tick_input={tickResult.TickInput} authority=server " +
            $"mode={NativePlayerSimulation.ControlModeName(state.ControlMode)} flags={input.Flags} controller={input.Controller} " +
            $"move=({input.MoveX:F3},{input.MoveY:F3},{input.MoveZ:F3}) " +
            $"client_camera=({input.CameraX:F3},{input.CameraY:F3},{input.CameraZ:F3},{input.CameraPitch:F6},{input.CameraYaw:F6}) " +
            $"pos=({state.X:F3},{state.Y:F3},{state.Z:F3}) " +
            $"delta=({tickResult.DeltaX:F3},{tickResult.DeltaY:F3},{tickResult.DeltaZ:F3}) " +
            $"pitch={state.Pitch:F6} yaw={state.Yaw:F6} " +
            $"velocity=({state.VelocityX:F3},{state.VelocityY:F3},{state.VelocityZ:F3}) " +
            $"ground={(state.IsOnGround ? 1 : 0)} saved={(persisted ? 1 : 0)}");
        LogMotionDiagnostics(in frame, state, tickResult);
    }

    private static void LogMotionDiagnostics(
        in HostFrameContext frame,
        PlayerState state,
        NativeTickResult tickResult)
    {
        var input = frame.Input;
        var inputMagnitude = MathF.Sqrt(
            (input.MoveX * input.MoveX) +
            (input.MoveY * input.MoveY) +
            (input.MoveZ * input.MoveZ));
        if (tickResult.TickInput == 0 || inputMagnitude <= 0.001f)
        {
            return;
        }

        var deltaMagnitude = MathF.Sqrt(
            (tickResult.DeltaX * tickResult.DeltaX) +
            (tickResult.DeltaY * tickResult.DeltaY) +
            (tickResult.DeltaZ * tickResult.DeltaZ));
        var cameraErrorX = input.CameraX - state.X;
        var cameraErrorY = input.CameraY - state.Y;
        var cameraErrorZ = input.CameraZ - state.Z;
        var horizontalCameraError = MathF.Sqrt(
            (cameraErrorX * cameraErrorX) + (cameraErrorZ * cameraErrorZ));
        var stalled = deltaMagnitude <= 0.001f ? 1 : 0;
        LiveDebugLog.Write(
            $"server_live_player_motion_profile frame={frame.FrameIndex} dt={frame.DeltaSeconds:F6} " +
            $"input_len={inputMagnitude:F3} delta_len={deltaMagnitude:F3} stalled={stalled} " +
            $"camera_error=({cameraErrorX:F3},{cameraErrorY:F3},{cameraErrorZ:F3}) " +
            $"horizontal_camera_error={horizontalCameraError:F3}");
    }

    private bool SaveIfDue(double deltaSeconds, bool force = false)
    {
        if (force) _saves.Flush();
        var changed = NoteCompletedSave();
        if (!_saves.HasCapacity) return changed;
        if (!_simulation.PrepareSave(_identity, deltaSeconds, force, out var state)) return changed;
        if (!_saves.TryEnqueue(state)) throw new InvalidOperationException("Player save queue admission changed on its owner thread.");
        if (force) { _saves.Flush(); changed |= NoteCompletedSave(); }
        return changed;
    }

    private bool NoteCompletedSave()
    {
        if (_saves.Completed is not { } receipt || receipt.Sequence <= _completedSave) return false;
        _simulation.NoteSaved(_identity, receipt.State);
        _completedSave = receipt.Sequence;
        return true;
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }
        try
        {
            SaveIfDue(0.0, force: true);
        }
        finally
        {
            _disposed = true;
            try { _saves.Dispose(); }
            finally { _simulation.Remove(_identity); }
        }
    }

    private void ThrowIfDisposed()
    {
        if (_disposed)
        {
            throw new ObjectDisposedException(nameof(PlayerController));
        }
    }

    private static PlayerState LoadInitialState(string playerDirectory, int playerId, out bool loadedFromSave)
    {
        if (NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(playerDirectory, playerId, out var saved) &&
            NativePlayerSimulation.TryCreateStateFromSave(saved, out var state))
        {
            loadedFromSave = true;
            return state;
        }

        loadedFromSave = false;
        return NativePlayerSimulation.DefaultState();
    }
}
