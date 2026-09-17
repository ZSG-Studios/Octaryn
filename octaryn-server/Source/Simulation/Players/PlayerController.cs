using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Simulation.Players;

internal sealed class PlayerController : IDisposable
{
    private readonly string _playerDirectory;
    private readonly PlayerSimulationWorld _simulation;
    private readonly PlayerSimulationIdentity _identity;
    private bool _disposed;

    public PlayerController(
        string playerDirectory,
        PlayerSimulationWorld simulation,
        int playerId = 1)
    {
        _playerDirectory = playerDirectory;
        _simulation = simulation;
        _identity = _simulation.Add(playerId,
            LoadInitialState(playerDirectory, playerId, out var loadedFromSave),
            loadedFromSave);
        var state = _simulation.Snapshot(_identity);
        LiveDebugLog.Write(
            $"server_live_player_load loaded={(loadedFromSave ? 1 : 0)} " +
            $"pos=({state.X:F3},{state.Y:F3},{state.Z:F3}) " +
            $"pitch={state.Pitch:F6} yaw={state.Yaw:F6} selected_block={state.SelectedBlock.Value}");
    }

    public PlayerState Snapshot()
    {
        ThrowIfDisposed();
        return _simulation.Snapshot(_identity);
    }

    public bool PlacementIntersectsPlayer(HostCommand command)
    {
        ThrowIfDisposed();
        if (command.Kind != HostCommandKind.SetBlock || command.D == 0)
        {
            return false;
        }

        return _simulation.Intersects(_identity, command.A, command.B, command.C);
    }

    public void AlignSpawnToSurface()
    {
        ThrowIfDisposed();
        var loadedFromSave = _simulation.LoadedFromSave(_identity);
        if (!_simulation.AlignSpawn(
            _identity,
            out var aligned,
            out var adjusted,
            out var surfaceY,
            out var surfaceBlock))
        {
            LiveDebugLog.Write(
                $"server_live_player_spawn_align active=0 reason=missing_surface " +
                $"loaded={(loadedFromSave ? 1 : 0)} pos=({aligned.X:F3},{aligned.Y:F3},{aligned.Z:F3})");
            return;
        }

        var persisted = SaveIfDue(0.0, force: true);
        LiveDebugLog.Write(
            $"server_live_player_spawn_align active=1 adjusted={(adjusted ? 1 : 0)} " +
            $"loaded={(loadedFromSave ? 1 : 0)} surface_y={surfaceY} surface_block={surfaceBlock.Value} " +
            $"eye_y={aligned.Y:F3} saved={(persisted ? 1 : 0)}");
    }

    public void Tick(in HostFrameContext frame)
    {
        if (ChunkStreamProcessBridge.CommandAuthorityActive)
        {
            ChunkStreamProcessBridge.ConsumePlayerCommands(Snapshot(), command => TickCommand(in command));
            return;
        }
        TickCommand(in frame);
    }

    private void TickCommand(in HostFrameContext frame)
    {
        var input = frame.Input;
        ThrowIfDisposed();
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
        return _simulation.SaveIfDue(_identity, _playerDirectory, deltaSeconds, force);
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
            _simulation.Remove(_identity);
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
