using Octaryn.Server.Persistence.Players;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;
using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.Simulation.Players;

internal sealed class PlayerController
{
    private const int PlayerId = 1;

    private readonly PlayerPersistence _persistence;
    private readonly NativePlayerSimulation _simulation;
    private NativePlayerSession _session;

    public PlayerController(
        PlayerPersistence persistence,
        BlockStore blocks,
        IBlockAuthorityRules blockRules,
        Func<BlockPosition, BlockId>? generatedBlocks = null)
    {
        _persistence = persistence;
        _simulation = new NativePlayerSimulation(blocks, blockRules, generatedBlocks);
        _session = NativePlayerSimulation.CreateSession(
            LoadInitialState(persistence, out var loadedFromSave),
            loadedFromSave);
        var state = NativePlayerSimulation.StateFromSession(_session);
        LiveDebugLog.Write(
            $"server_live_player_load loaded={(loadedFromSave ? 1 : 0)} " +
            $"pos=({state.X:F3},{state.Y:F3},{state.Z:F3}) " +
            $"pitch={state.Pitch:F6} yaw={state.Yaw:F6} selected_block={state.SelectedBlock.Value}");
    }

    public PlayerState Snapshot()
    {
        return NativePlayerSimulation.StateFromSession(_session);
    }

    public void AlignSpawnToSurface()
    {
        var loadedFromSave = NativePlayerSimulation.SessionLoadedFromSave(_session);
        if (!_simulation.TryAlignSpawnToSurface(
            ref _session,
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
        var input = frame.Input;
        var state = _simulation.Step(ref _session, input, frame.DeltaSeconds, out var tickResult);
        var persisted = SaveIfDue(frame.DeltaSeconds);
        LiveDebugLog.Write(
            $"server_live_player_state frame={frame.FrameIndex} tick_input={tickResult.TickInput} authority=server " +
            $"mode={ModeName(state.ControlMode)} flags={input.Flags} controller={input.Controller} " +
            $"move=({input.MoveX:F3},{input.MoveY:F3},{input.MoveZ:F3}) " +
            $"client_camera=({input.CameraX:F3},{input.CameraY:F3},{input.CameraZ:F3},{input.CameraPitch:F6},{input.CameraYaw:F6}) " +
            $"pos=({state.X:F3},{state.Y:F3},{state.Z:F3}) " +
            $"delta=({tickResult.DeltaX:F3},{tickResult.DeltaY:F3},{tickResult.DeltaZ:F3}) " +
            $"pitch={state.Pitch:F6} yaw={state.Yaw:F6} " +
            $"velocity=({state.VelocityX:F3},{state.VelocityY:F3},{state.VelocityZ:F3}) " +
            $"ground={(state.IsOnGround ? 1 : 0)} saved={(persisted ? 1 : 0)}");
    }

    private bool SaveIfDue(double deltaSeconds, bool force = false)
    {
        var decision = NativePlayerSimulation.SaveDecision(ref _session, deltaSeconds, force);
        if (decision.ShouldSave == 0)
        {
            return false;
        }

        var saveState = NativePlayerSimulation.SaveStateFromSessionSaveResult(decision);
        _persistence.Save(PlayerId, saveState);
        NativePlayerSimulation.NoteSaved(ref _session, saveState);
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

    private static string ModeName(PlayerControlMode mode)
    {
        return mode == PlayerControlMode.Fly ? "fly" : "walk";
    }
}
