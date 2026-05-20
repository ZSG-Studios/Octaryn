using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Globalization;
using Octaryn.Server.Host;
using Octaryn.Server.Modules;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Chunks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static unsafe class ChunkStreamProcessBridge
{
    private static readonly IntPtr s_streamWriteTracker = NativeBlockStoreLibrary.ChunkStreamWriteTrackerCreate();
    private static readonly IntPtr s_blockInteractionFrameTracker =
        NativeBlockStoreLibrary.BlockInteractionFrameTrackerCreate();
    private static readonly Stopwatch s_liveTickClock = Stopwatch.StartNew();
    private static long s_lastPlayerTickTimestamp;
    private static long s_lastPlayerStateStreamWriteTimestamp;
    private const double PlayerStateStreamIntervalSeconds = 1.0 / 60.0;

    public static int HandleIfRequested(ModuleActivator gameModule, bool allowMissingIntent = false)
    {
        var paths = NativeHostPolicyLibrary.GetLiveStreamPaths();
        var requestPlan = NativeHostPolicyLibrary.PlanLiveStreamRequest(paths);
        if (!requestPlan.ShouldHandle)
        {
            return requestPlan.HandleResult;
        }
        if (!requestPlan.ShouldContinue)
        {
            LogLiveStreamRequestStopReason(requestPlan);
            return requestPlan.HandleResult;
        }

        var intentPath = paths.ChunkViewIntentPath!;
        var streamPath = paths.ChunkStreamPath!;

        var intent = default(NativeChunkViewIntent);
        var intentPlan = default(NativeChunkStreamProcessWritePlan);
        if (ReadProcessChunkViewIntent(intentPath, allowMissingIntent, &intent, &intentPlan) != 0)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=intent_read_failed path={intentPath}");
            return -1;
        }
        if (intentPlan.ShouldContinue == 0)
        {
            LogChunkStreamPlanStopReason(intentPath, intentPlan);
            return intentPlan.HandleResult;
        }

        if (!TryReadPlayerInputIntent(paths.PlayerInputIntentPath, allowMissingIntent, out var frame, out var hasPlayerInput))
        {
            return -1;
        }

        ApplyWorldTimeIntentIfRequested(gameModule, paths.WorldTimeIntentPath);
        var metadataOnly = paths.MetadataOnly;

        if (!ApplyBlockInteractionIntentIfRequested(gameModule, paths.BlockInteractionIntentPath, allowMissingIntent, out var submittedBlockCommands))
        {
            return -1;
        }

        var stagePlan = default(NativeChunkStreamProcessStagePlan);
        if (NativeBlockStoreLibrary.ChunkStreamPlanProcessStage(
                StreamWriteTracker,
                &intent,
                hasPlayerInput ? 1u : 0u,
                submittedBlockCommands ? 1u : 0u,
                metadataOnly ? 1u : 0u,
                &stagePlan) != 0)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=intent_read_failed path={intentPath}");
            return -1;
        }
        if (ChunkStreamProcessTickBridge.Execute(gameModule, in frame, stagePlan.Tick) != 0)
        {
            return -1;
        }
        var player = gameModule.SnapshotPlayer();
        if (!TryWritePlayerStateStream(paths.PlayerStateStreamPath, frame.Timing.FrameIndex, player))
        {
            return -1;
        }

        var writePlan = stagePlan.Write;
        if (writePlan.ShouldContinue == 0)
        {
            LogChunkStreamPlanStopReason(intentPath, writePlan);
            return writePlan.HandleResult;
        }

        LiveDebugLog.Write($"server_live_chunk_view_intent source=process_file path={intentPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
        if (writePlan.ShouldWrite == 0)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=1 skipped=1 reason=unchanged_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
            return 0;
        }

        var worldTime = gameModule.SnapshotWorldTime();
        var writesAuthoritativeEdits = submittedBlockCommands;
        var effectiveMetadataOnly = metadataOnly && !writesAuthoritativeEdits;
        var writeResult = gameModule.WriteChunkStreamProcessSnapshotFile(
            StreamWriteTracker,
            streamPath,
            intent,
            writePlan,
            effectiveMetadataOnly,
            worldTime,
            player);

        LiveDebugLog.Write($"server_live_chunk_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} load={writeResult.LoadCount} preserve={writeResult.PreserveCount} unload={writeResult.UnloadCount}");
        LiveDebugLog.Write($"server_live_chunk_stream active=1 source=process_file path={streamPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} columns={writeResult.Counts.ColumnCount} blocks={writeResult.Counts.BlockCount} metadata_only={(effectiveMetadataOnly ? 1 : 0)} requested_metadata_only={(metadataOnly ? 1 : 0)} command_delta={(writesAuthoritativeEdits ? 1 : 0)} world_time_day_fraction={worldTime.DayFraction:F6}");
        return 0;
    }

    private static IntPtr StreamWriteTracker =>
        s_streamWriteTracker != IntPtr.Zero
            ? s_streamWriteTracker
            : throw new InvalidOperationException("Native chunk stream write tracker allocation failed.");

    private static IntPtr BlockInteractionFrameTracker =>
        s_blockInteractionFrameTracker != IntPtr.Zero
            ? s_blockInteractionFrameTracker
            : throw new InvalidOperationException("Native block interaction frame tracker allocation failed.");

    private static void LogLiveStreamRequestStopReason(NativeHostLiveStreamRequestPlan plan)
    {
        var reason = NativeHostPolicyLibrary.LiveStreamRequestReasonName(plan.Reason);
        LiveDebugLog.Write($"server_live_chunk_stream active=0 reason={reason}");
    }

    private static int ReadProcessChunkViewIntent(
        string path,
        bool allowTransientInvalid,
        NativeChunkViewIntent* intent,
        NativeChunkStreamProcessWritePlan* plan)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            return NativeBlockStoreLibrary.ChunkStreamReadProcessIntent(
                (byte*)pathPointer,
                allowTransientInvalid ? 1u : 0u,
                intent,
                plan);
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    private static void LogChunkStreamPlanStopReason(string path, NativeChunkStreamProcessWritePlan plan)
    {
        var text = NativeText(
            NativeBlockStoreLibrary.ChunkStreamProcessWriteReasonName(plan.Reason, plan.HandleResult));
        LiveDebugLog.Write($"server_live_chunk_stream active=0 reason={text} path={path}");
    }

    private static void ApplyWorldTimeIntentIfRequested(ModuleActivator gameModule, string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        var nativeIntent = stackalloc NativeWorldTimeIntent[1];
        var nativePlan = stackalloc NativeWorldTimeIntentProcessPlan[1];
        int readResult;
        try
        {
            readResult = NativeWorldTimeLibrary.ReadIntentFile((byte*)pathPointer, nativeIntent);
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
        if (NativeWorldTimeLibrary.PlanIntent(readResult, nativeIntent, nativePlan) != 0)
        {
            LiveDebugLog.Write($"server_live_world_time_intent active=0 reason=invalid_intent path={path}");
            return;
        }
        var plan = nativePlan[0];
        if (plan.ShouldApply == 0)
        {
            LogWorldTimeIntentPlanStopReason(path, plan);
            return;
        }

        var intent = nativeIntent[0];
        gameModule.SetWorldTimeSpeedMultiplier(intent.SpeedMultiplier);
        LiveDebugLog.Write($"server_live_world_time_intent active=1 source=process_file path={path} speed_index={intent.SpeedIndex} speed_multiplier={intent.SpeedMultiplier:F3}");
    }

    private static void LogWorldTimeIntentPlanStopReason(string path, NativeWorldTimeIntentProcessPlan plan)
    {
        var reason = NativeWorldTimeLibrary.ProcessReasonName(plan.Reason);
        if (reason == "missing_intent")
        {
            return;
        }

        LiveDebugLog.Write($"server_live_world_time_intent active=0 reason={reason} path={path}");
    }

    private static bool TryReadPlayerInputIntent(string? playerInputIntentPath, bool allowTransientInvalid, out HostFrameSnapshot frame, out bool shouldTick)
    {
        frame = default;
        shouldTick = false;
        if (string.IsNullOrWhiteSpace(playerInputIntentPath))
        {
            return true;
        }

        if (NativePlayerSimulation.ReadProcessInputIntent(
                playerInputIntentPath,
                allowTransientInvalid,
                out var result) != 0)
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=intent_read_failed path={playerInputIntentPath}");
            return false;
        }
        var intent = result.Intent;
        var plan = result.Plan;
        if (plan.ShouldContinue == 0)
        {
            LogPlayerInputPlanStopReason(playerInputIntentPath, plan);
            return false;
        }
        if (plan.ShouldTick == 0)
        {
            LogPlayerInputPlanStopReason(playerInputIntentPath, plan);
            return true;
        }

        LiveDebugLog.Write(
            $"server_live_player_input_intent active=1 source=process_file path={playerInputIntentPath} " +
            $"frame={intent.FrameIndex} dt={intent.DeltaSeconds:F6} flags={intent.Input.Flags} controller={intent.Input.Controller} " +
            $"move=({intent.Input.MoveX:F3},{intent.Input.MoveY:F3},{intent.Input.MoveZ:F3}) " +
            $"camera=({intent.Input.CameraX:F3},{intent.Input.CameraY:F3},{intent.Input.CameraZ:F3},{intent.Input.CameraPitch:F6},{intent.Input.CameraYaw:F6})");
        frame = WithServerElapsedDelta(result.Frame, intent.DeltaSeconds);
        shouldTick = true;
        return true;
    }

    private static HostFrameSnapshot WithServerElapsedDelta(HostFrameSnapshot frame, double clientDeltaSeconds)
    {
        var now = s_liveTickClock.ElapsedTicks;
        var previous = s_lastPlayerTickTimestamp;
        s_lastPlayerTickTimestamp = now;
        var deltaSeconds = previous == 0
            ? 1.0 / 60.0
            : (now - previous) / (double)Stopwatch.Frequency;
        deltaSeconds = Math.Clamp(deltaSeconds, 1.0 / 1000.0, 0.25);
        LiveDebugLog.Write(
            $"server_live_player_tick_timing frame={frame.Timing.FrameIndex} " +
            $"client_dt={clientDeltaSeconds:F6} server_elapsed_dt={deltaSeconds:F6}");
        return new HostFrameSnapshot(
            frame.Input,
            new HostFrameTimingSnapshot(
                frame.Timing.Version,
                frame.Timing.Size,
                frame.Timing.FrameIndex,
                deltaSeconds));
    }

    private static bool TryWritePlayerStateStream(string? path, ulong frameIndex, PlayerState player)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return true;
        }

        var now = s_liveTickClock.ElapsedTicks;
        if (s_lastPlayerStateStreamWriteTimestamp != 0)
        {
            var elapsed = (now - s_lastPlayerStateStreamWriteTimestamp) / (double)Stopwatch.Frequency;
            if (elapsed < PlayerStateStreamIntervalSeconds)
            {
                return true;
            }
        }
        s_lastPlayerStateStreamWriteTimestamp = now;

        try
        {
            var directory = Path.GetDirectoryName(path);
            if (!string.IsNullOrWhiteSpace(directory))
            {
                Directory.CreateDirectory(directory);
            }
            var tempPath = path + ".tmp";
            var payload = string.Create(
                CultureInfo.InvariantCulture,
                $"{{\"version\":1,\"source\":\"server_player_state_stream\",\"frameIndex\":{frameIndex}," +
                $"\"playerX\":{player.X:R},\"playerY\":{player.Y:R},\"playerZ\":{player.Z:R}," +
                $"\"playerPitch\":{player.Pitch:R},\"playerYaw\":{player.Yaw:R}," +
                $"\"playerVelocityX\":{player.VelocityX:R},\"playerVelocityY\":{player.VelocityY:R}," +
                $"\"playerVelocityZ\":{player.VelocityZ:R},\"playerControlMode\":{player.ControlMode}," +
                $"\"playerOnGround\":{(player.IsOnGround ? 1 : 0)}}}");
            File.WriteAllText(tempPath, payload);
            File.Move(tempPath, path, overwrite: true);
            return true;
        }
        catch (Exception ex)
        {
            LiveDebugLog.Write($"server_live_player_state_stream active=0 reason=write_failed error={ex.GetType().Name}");
            return false;
        }
    }

    private static void LogPlayerInputPlanStopReason(string path, NativeInputProcessPlan plan)
    {
        var reason = NativePlayerSimulation.InputProcessReasonName(plan.Reason);
        LiveDebugLog.Write($"server_live_player_input_intent active=0 reason={reason} path={path}");
    }

    private static bool ApplyBlockInteractionIntentIfRequested(ModuleActivator gameModule, string? blockInteractionIntentPath, bool allowTransientInvalid, out bool submittedBlockCommands)
    {
        submittedBlockCommands = false;
        if (string.IsNullOrWhiteSpace(blockInteractionIntentPath))
        {
            return true;
        }

        var commands = new HostCommand[ClientBlockCommandQueue.MaxPendingCommands];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(blockInteractionIntentPath);
        var intent = default(NativeBlockInteractionIntentResult);
        var plan = default(NativeBlockInteractionProcessPlan);
        int readResult;
        fixed (HostCommand* commandPointer = commands)
        {
            try
            {
                readResult = NativeBlockStoreLibrary.BlockInteractionReadIntentFile(
                    (byte*)pathPointer,
                    commandPointer,
                    (uint)commands.Length,
                    &intent);
            }
            finally
            {
                Marshal.FreeCoTaskMem(pathPointer);
            }
        }
        if (NativeBlockStoreLibrary.BlockInteractionPlanProcessIntent(
                BlockInteractionFrameTracker,
                readResult,
                allowTransientInvalid ? 1u : 0u,
                &intent,
                &plan) != 0)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=intent_read_failed path={blockInteractionIntentPath}");
            return false;
        }
        if (plan.ShouldContinue == 0)
        {
            LogBlockInteractionPlanStopReason(blockInteractionIntentPath, plan);
            return false;
        }
        if (plan.ShouldSubmit == 0)
        {
            LogBlockInteractionPlanStopReason(blockInteractionIntentPath, plan);
            return true;
        }

        LiveDebugLog.Write(
            $"server_live_block_interaction_intent active=1 source=process_file path={blockInteractionIntentPath} " +
            $"frame={plan.FrameIndex} commands={plan.CommandCount} break={plan.BreakCommandCount} place={plan.PlaceCommandCount}");

        int submitResult;
        fixed (HostCommand* commandPointer = commands)
        {
            submitResult = gameModule.SubmitClientCommands(commandPointer, plan.CommandCount);
        }
        LiveDebugLog.Write($"server_live_block_interaction_submit result={submitResult} commands={plan.CommandCount}");

        NativeBlockStoreLibrary.BlockInteractionFrameTrackerNoteSubmitted(
            BlockInteractionFrameTracker,
            plan.FrameIndex);
        if (submitResult == 0)
        {
            TryClearSubmittedBlockInteractionIntent(blockInteractionIntentPath);
        }
        submittedBlockCommands = submitResult == 0 && plan.CommandCount > 0;
        return true;
    }

    private static void TryClearSubmittedBlockInteractionIntent(string path)
    {
        try
        {
            System.IO.File.Delete(path);
        }
        catch (Exception ex)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent_clear active=0 reason=delete_failed path={path} error={ex.GetType().Name}");
        }
    }

    private static void LogBlockInteractionPlanStopReason(string path, NativeBlockInteractionProcessPlan plan)
    {
        var reason = NativeText(NativeBlockStoreLibrary.BlockInteractionProcessReasonName(plan.Reason));
        if (reason is "waiting_for_intent" or "duplicate_frame")
        {
            return;
        }
        LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason={reason} path={path}");
    }

    private static string NativeText(byte* value)
    {
        return Marshal.PtrToStringUTF8((IntPtr)value) ?? "intent_read_failed";
    }

}
