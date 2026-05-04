using Octaryn.Server.Persistence.Players;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;
using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.Simulation.Players;

internal sealed class ServerPlayerController
{
    private const int PlayerId = 1;
    private const float DefaultSpawnY = 80.0f;
    private const float DefaultSpawnPitch = -0.35f;
    private const ushort DefaultSelectedBlock = 25;
    private const float NormalFlySpeedBlocksPerSecond = 10.0f;
    private const float SprintFlySpeedBlocksPerSecond = 100.0f;
    private const float WalkSpeedBlocksPerSecond = 5.0f;
    private const float SprintWalkSpeedBlocksPerSecond = 9.0f;
    private const float MaxDeltaSeconds = 0.05f;
    private const float PositionPersistEpsilon = 0.01f;
    private const float AnglePersistEpsilon = 0.001f;
    private const float Pi = MathF.PI;
    private const float TwoPi = MathF.PI * 2.0f;

    private readonly ServerPlayerPersistence _persistence;
    private readonly ServerPlayerCollision _collision;
    private ServerPlayerState _state;
    private ServerPlayerSaveState _lastSaved;
    private bool _loadedFromSave;

    public ServerPlayerController(
        ServerPlayerPersistence persistence,
        ServerBlockStore blocks,
        IBlockAuthorityRules blockRules)
    {
        _persistence = persistence;
        _collision = new ServerPlayerCollision(blocks, blockRules);
        _state = LoadInitialState(persistence, out _loadedFromSave);
        _lastSaved = ToSaveState(_state);
        ServerLiveDebugLog.Write(
            $"server_live_player_load loaded={(_loadedFromSave ? 1 : 0)} " +
            $"pos=({_state.X:F3},{_state.Y:F3},{_state.Z:F3}) " +
            $"pitch={_state.Pitch:F6} yaw={_state.Yaw:F6} selected_block={_state.SelectedBlock.Value}");
    }

    public ServerPlayerState Snapshot()
    {
        return _state;
    }

    public void AlignSpawnToSurface()
    {
        var before = _state;
        if (!_collision.TryAlignSpawnToSurface(
            _state,
            _loadedFromSave,
            out var aligned,
            out var surfaceY,
            out var surfaceBlock))
        {
            ServerLiveDebugLog.Write(
                $"server_live_player_spawn_align active=0 reason=missing_surface " +
                $"loaded={(_loadedFromSave ? 1 : 0)} pos=({_state.X:F3},{_state.Y:F3},{_state.Z:F3})");
            return;
        }

        if (!_loadedFromSave)
        {
            aligned = aligned with { Pitch = DefaultSpawnPitch };
        }

        _state = aligned;
        var persisted = SaveIfChanged(_state);
        ServerLiveDebugLog.Write(
            $"server_live_player_spawn_align active=1 adjusted={(MathF.Abs(_state.Y - before.Y) > PositionPersistEpsilon ? 1 : 0)} " +
            $"loaded={(_loadedFromSave ? 1 : 0)} surface_y={surfaceY} surface_block={surfaceBlock.Value} " +
            $"eye_y={_state.Y:F3} saved={(persisted ? 1 : 0)}");
        _loadedFromSave = true;
    }

    public void Tick(in HostFrameContext frame)
    {
        var input = frame.Input;
        var before = _state;
        if (!HasInputIntent(input))
        {
            _state = _state with
            {
                VelocityX = 0.0f,
                VelocityY = 0.0f,
                VelocityZ = 0.0f
            };
            ServerLiveDebugLog.Write(
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

        var mode = input.FlyMode ? ServerPlayerControlMode.Fly : ServerPlayerControlMode.Walk;
        var pitch = FiniteOr(input.CameraPitch, _state.Pitch);
        var yaw = NormalizeYaw(FiniteOr(input.CameraYaw, _state.Yaw));
        var dt = ClampDeltaSeconds(frame.DeltaSeconds);

        _state = mode == ServerPlayerControlMode.Fly
            ? ApplyFlyInput(input, dt, pitch, yaw)
            : ApplyWalkInput(input, dt, pitch, yaw);

        var persisted = SaveIfChanged(_state);
        ServerLiveDebugLog.Write(
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

    private ServerPlayerState ApplyFlyInput(HostInputSnapshot input, float dt, float pitch, float yaw)
    {
        var speed = input.Sprint ? SprintFlySpeedBlocksPerSecond : NormalFlySpeedBlocksPerSecond;
        var distance = speed * dt;
        var move = MoveCameraRelative(input.MoveX * distance, input.MoveY * distance, input.MoveZ * distance, pitch, yaw);
        return _state with
        {
            X = _state.X + move.X,
            Y = Math.Clamp(_state.Y + move.Y, -1000.0f, 1000.0f),
            Z = _state.Z + move.Z,
            Pitch = ClampPitch(pitch),
            Yaw = yaw,
            VelocityX = dt > 0.0f ? move.X / dt : 0.0f,
            VelocityY = dt > 0.0f ? move.Y / dt : 0.0f,
            VelocityZ = dt > 0.0f ? move.Z / dt : 0.0f,
            IsOnGround = false,
            ControlMode = ServerPlayerControlMode.Fly
        };
    }

    private ServerPlayerState ApplyWalkInput(HostInputSnapshot input, float dt, float pitch, float yaw)
    {
        return _collision.MoveWalk(
            _state,
            input,
            dt,
            ClampPitch(pitch),
            yaw,
            WalkSpeedBlocksPerSecond,
            SprintWalkSpeedBlocksPerSecond);
    }

    private bool SaveIfChanged(ServerPlayerState state)
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

    private static ServerPlayerState LoadInitialState(ServerPlayerPersistence persistence, out bool loadedFromSave)
    {
        if (persistence.TryLoad(PlayerId, out var saved) &&
            float.IsFinite(saved.X) &&
            float.IsFinite(saved.Y) &&
            float.IsFinite(saved.Z) &&
            float.IsFinite(saved.Pitch) &&
            float.IsFinite(saved.Yaw))
        {
            loadedFromSave = true;
            return new ServerPlayerState(
                saved.X,
                Math.Clamp(saved.Y, -1000.0f, 1000.0f),
                saved.Z,
                ClampPitch(saved.Pitch),
                NormalizeYaw(saved.Yaw),
                0.0f,
                0.0f,
                0.0f,
                false,
                ServerPlayerControlMode.Walk,
                saved.SelectedBlock);
        }

        loadedFromSave = false;
        return new ServerPlayerState(
            0.0f,
            DefaultSpawnY,
            0.0f,
            DefaultSpawnPitch,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            false,
            ServerPlayerControlMode.Walk,
            new BlockId(DefaultSelectedBlock));
    }

    private static ServerPlayerSaveState ToSaveState(ServerPlayerState state)
    {
        return new ServerPlayerSaveState(
            state.X,
            state.Y,
            state.Z,
            state.Pitch,
            state.Yaw,
            state.SelectedBlock);
    }

    private static bool SaveStatesMatch(ServerPlayerSaveState left, ServerPlayerSaveState right)
    {
        return MathF.Abs(left.X - right.X) <= PositionPersistEpsilon &&
            MathF.Abs(left.Y - right.Y) <= PositionPersistEpsilon &&
            MathF.Abs(left.Z - right.Z) <= PositionPersistEpsilon &&
            MathF.Abs(left.Pitch - right.Pitch) <= AnglePersistEpsilon &&
            MathF.Abs(left.Yaw - right.Yaw) <= AnglePersistEpsilon &&
            left.SelectedBlock == right.SelectedBlock;
    }

    private static bool HasInputIntent(HostInputSnapshot input)
    {
        return input.Controller != 0 ||
            input.Flags != 0 ||
            input.MoveX != 0.0f ||
            input.MoveY != 0.0f ||
            input.MoveZ != 0.0f ||
            input.RelativeMouse != 0;
    }

    private static float ClampDeltaSeconds(double value)
    {
        if (!double.IsFinite(value) || value <= 0.0)
        {
            return 0.0f;
        }

        return (float)Math.Min(value, MaxDeltaSeconds);
    }

    private static float ClampPitch(float pitch)
    {
        return Math.Clamp(pitch, -Pi * 0.5f + float.Epsilon, Pi * 0.5f - float.Epsilon);
    }

    private static float NormalizeYaw(float yaw)
    {
        yaw = (yaw + Pi) % TwoPi;
        if (yaw < 0.0f)
        {
            yaw += TwoPi;
        }

        return yaw - Pi;
    }

    private static float FiniteOr(float value, float fallback)
    {
        return float.IsFinite(value) ? value : fallback;
    }

    private static string ModeName(ServerPlayerControlMode mode)
    {
        return mode == ServerPlayerControlMode.Fly ? "fly" : "walk";
    }

    private static (float X, float Y, float Z) MoveCameraRelative(float x, float y, float z, float pitch, float yaw)
    {
        var yawSine = MathF.Sin(yaw);
        var yawCosine = MathF.Cos(yaw);
        var pitchSine = MathF.Sin(pitch);
        var pitchCosine = MathF.Cos(pitch);
        return (
            pitchCosine * (yawSine * z) + yawCosine * x,
            y + z * pitchSine,
            -(pitchCosine * (yawCosine * z) - yawSine * x));
    }

}
