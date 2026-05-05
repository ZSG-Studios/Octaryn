using Octaryn.Server.Persistence.Players;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;
using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.Simulation.Players;

internal sealed class PlayerController
{
    private const int PlayerId = 1;
    private const float PositionPersistEpsilon = 0.01f;
    private const float AnglePersistEpsilon = 0.001f;

    private readonly PlayerPersistence _persistence;
    private readonly NativePlayerSimulation _simulation;
    private PlayerState _state;
    private PlayerSaveState _lastSaved;
    private bool _loadedFromSave;

    public PlayerController(
        PlayerPersistence persistence,
        BlockStore blocks,
        IBlockAuthorityRules blockRules,
        Func<BlockPosition, BlockId>? generatedBlocks = null)
    {
        _persistence = persistence;
        _simulation = new NativePlayerSimulation(blocks, blockRules, generatedBlocks);
        _state = LoadInitialState(persistence, out _loadedFromSave);
        _lastSaved = ToSaveState(_state);
        LiveDebugLog.Write(
            $"server_live_player_load loaded={(_loadedFromSave ? 1 : 0)} " +
            $"pos=({_state.X:F3},{_state.Y:F3},{_state.Z:F3}) " +
            $"pitch={_state.Pitch:F6} yaw={_state.Yaw:F6} selected_block={_state.SelectedBlock.Value}");
    }

    public PlayerState Snapshot()
    {
        return _state;
    }

    public void AlignSpawnToSurface()
    {
        var before = _state;
        if (!_simulation.TryAlignSpawnToSurface(
            _state,
            _loadedFromSave,
            out var aligned,
            out var surfaceY,
            out var surfaceBlock))
        {
            LiveDebugLog.Write(
                $"server_live_player_spawn_align active=0 reason=missing_surface " +
                $"loaded={(_loadedFromSave ? 1 : 0)} pos=({_state.X:F3},{_state.Y:F3},{_state.Z:F3})");
            return;
        }

        _state = aligned;
        var persisted = SaveIfChanged(_state);
        LiveDebugLog.Write(
            $"server_live_player_spawn_align active=1 adjusted={(MathF.Abs(_state.Y - before.Y) > PositionPersistEpsilon ? 1 : 0)} " +
            $"loaded={(_loadedFromSave ? 1 : 0)} surface_y={surfaceY} surface_block={surfaceBlock.Value} " +
            $"eye_y={_state.Y:F3} saved={(persisted ? 1 : 0)}");
        _loadedFromSave = true;
    }

    public void Tick(in HostFrameContext frame)
    {
        var input = frame.Input;
        var before = _state;
        if (!NativePlayerSimulation.HasInputIntent(input))
        {
            _state = _simulation.Idle(_state);
            LiveDebugLog.Write(
                $"server_live_player_state frame={frame.FrameIndex} tick_input=0 authority=server " +
                $"mode={ModeName(_state.ControlMode)} flags={input.Flags} controller={input.Controller} " +
                $"move=({input.MoveX:F3},{input.MoveY:F3},{input.MoveZ:F3}) " +
                $"client_camera=({input.CameraX:F3},{input.CameraY:F3},{input.CameraZ:F3},{input.CameraPitch:F6},{input.CameraYaw:F6}) " +
                $"pos=({_state.X:F3},{_state.Y:F3},{_state.Z:F3}) " +
                $"delta=(0.000,0.000,0.000) pitch={_state.Pitch:F6} yaw={_state.Yaw:F6} " +
                $"velocity=({_state.VelocityX:F3},{_state.VelocityY:F3},{_state.VelocityZ:F3}) " +
                $"ground={(_state.IsOnGround ? 1 : 0)} saved=0");
            return;
        }

        _state = _simulation.Move(_state, input, frame.DeltaSeconds);

        var persisted = SaveIfChanged(_state);
        LiveDebugLog.Write(
            $"server_live_player_state frame={frame.FrameIndex} tick_input=1 authority=server " +
            $"mode={ModeName(_state.ControlMode)} flags={input.Flags} controller={input.Controller} " +
            $"move=({input.MoveX:F3},{input.MoveY:F3},{input.MoveZ:F3}) " +
            $"client_camera=({input.CameraX:F3},{input.CameraY:F3},{input.CameraZ:F3},{input.CameraPitch:F6},{input.CameraYaw:F6}) " +
            $"pos=({_state.X:F3},{_state.Y:F3},{_state.Z:F3}) " +
            $"delta=({_state.X - before.X:F3},{_state.Y - before.Y:F3},{_state.Z - before.Z:F3}) " +
            $"pitch={_state.Pitch:F6} yaw={_state.Yaw:F6} " +
            $"velocity=({_state.VelocityX:F3},{_state.VelocityY:F3},{_state.VelocityZ:F3}) " +
            $"ground={(_state.IsOnGround ? 1 : 0)} saved={(persisted ? 1 : 0)}");
    }

    private bool SaveIfChanged(PlayerState state)
    {
        var saveState = ToSaveState(state);
        if (SaveStatesMatch(_lastSaved, saveState))
        {
            return false;
        }

        _persistence.Save(PlayerId, saveState);
        _lastSaved = saveState;
        return true;
    }

    private static PlayerState LoadInitialState(PlayerPersistence persistence, out bool loadedFromSave)
    {
        if (persistence.TryLoad(PlayerId, out var saved) &&
            NativePlayerSimulation.TryCreateStateFromSave(saved, out var state))
        {
            loadedFromSave = true;
            return state;
        }

        loadedFromSave = false;
        return NativePlayerSimulation.DefaultState();
    }

    private static PlayerSaveState ToSaveState(PlayerState state)
    {
        return new PlayerSaveState(
            state.X,
            state.Y,
            state.Z,
            state.Pitch,
            state.Yaw,
            state.SelectedBlock);
    }

    private static bool SaveStatesMatch(PlayerSaveState left, PlayerSaveState right)
    {
        return MathF.Abs(left.X - right.X) <= PositionPersistEpsilon &&
            MathF.Abs(left.Y - right.Y) <= PositionPersistEpsilon &&
            MathF.Abs(left.Z - right.Z) <= PositionPersistEpsilon &&
            MathF.Abs(left.Pitch - right.Pitch) <= AnglePersistEpsilon &&
            MathF.Abs(left.Yaw - right.Yaw) <= AnglePersistEpsilon &&
            left.SelectedBlock == right.SelectedBlock;
    }

    private static string ModeName(PlayerControlMode mode)
    {
        return mode == PlayerControlMode.Fly ? "fly" : "walk";
    }
}
