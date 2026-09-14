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

internal static unsafe partial class ChunkStreamProcessBridge
{
    private static readonly IntPtr s_streamWriteTracker = NativeBlockStoreLibrary.ChunkStreamWriteTrackerCreate();
    private static readonly IntPtr s_blockInteractionFrameTracker =
        NativeBlockStoreLibrary.BlockInteractionFrameTrackerCreate();
    private static readonly Stopwatch s_liveTickClock = Stopwatch.StartNew();
    private static long s_lastPlayerTickTimestamp;
    private static readonly PlayerStatePublicationClock s_playerPublication = new();
    private static ulong s_sourceTick;
    private static double s_sourceSeconds;
    private static long s_snapshotContentionCount;

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

        gameModule.SetFluidRegion(intent.CenterChunkX, intent.CenterChunkZ, intent.Radius);
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
        if (ExecuteTrackedPlayerTick(gameModule, in frame, stagePlan.Tick) != 0)
        {
            return -1;
        }
        var player = gameModule.SnapshotPlayer();
        var playerWorldTime = gameModule.SnapshotWorldTime();
        if (!TryWritePlayerStateStream(paths.PlayerStateStreamPath, frame.Timing.FrameIndex, player,
            playerWorldTime.DayFraction, playerWorldTime.TotalWorldSeconds))
        {
            return -1;
        }
        var liveProcess = NativeHostPolicyLibrary.GetStartupPolicy().LiveProcessStream;
        return PublishSnapshot(gameModule, streamPath, intent, metadataOnly, submittedBlockCommands, liveProcess);
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
        var processFrame = NativeHostPolicyLibrary.GetStartupPolicy().LiveProcessStream
            ? PlayerInputFreshness.Apply(result.Frame) : result.Frame;
        frame = WithServerElapsedDelta(processFrame, intent.DeltaSeconds);
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

    private static bool TryWritePlayerStateStream(string? path, ulong frameIndex, PlayerState player,
        float worldDayFraction, double worldTotalSeconds)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return true;
        }

        var now = s_liveTickClock.Elapsed.TotalSeconds;
        if (!s_playerPublication.ShouldPublish(now, s_sourceTick)) return true;

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
                $"\"sourceTick\":{s_sourceTick},\"sourceSeconds\":{s_sourceSeconds:R}," +
                $"\"worldTimeDayFraction\":{worldDayFraction:R},\"worldTimeTotalSeconds\":{worldTotalSeconds:R}," +
                $"\"playerX\":{player.X:R},\"playerY\":{player.Y:R},\"playerZ\":{player.Z:R}," +
                $"\"playerPitch\":{player.Pitch:R},\"playerYaw\":{player.Yaw:R}," +
                $"\"playerVelocityX\":{player.VelocityX:R},\"playerVelocityY\":{player.VelocityY:R}," +
                $"\"playerVelocityZ\":{player.VelocityZ:R},\"playerControlMode\":{player.ControlMode}," +
                $"\"playerOnGround\":{(player.IsOnGround ? 1 : 0)}}}");
            File.WriteAllText(tempPath, payload);
            File.Move(tempPath, path, overwrite: true);
            s_playerPublication.Published(now, s_sourceTick);
            if (s_snapshotContentionCount != 0)
            {
                LiveDebugLog.Write($"server_live_player_state_stream active=1 recovered=1 skipped={s_snapshotContentionCount}");
                s_snapshotContentionCount = 0;
            }
            return true;
        }
        catch (Exception ex) when (NativeHostPolicyLibrary.GetStartupPolicy().LiveProcessStream &&
            OperatingSystem.IsWindows() && ex is IOException or UnauthorizedAccessException &&
            (ex.HResult & 0xffff) is 5 or 32 or 33)
        {
            // Keep the last complete snapshot; the next tick publishes fresh state without a retry queue.
            if (++s_snapshotContentionCount == 1 || s_snapshotContentionCount % 300 == 0)
                LiveDebugLog.Write($"server_live_player_state_stream active=1 deferred=1 reason=file_contention error={ex.GetType().Name} code={ex.HResult & 0xffff} skipped={s_snapshotContentionCount}");
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
            if (NativeText(NativeBlockStoreLibrary.BlockInteractionProcessReasonName(plan.Reason)) == "duplicate_frame")
            {
                TryClearSubmittedBlockInteractionIntent(blockInteractionIntentPath);
            }
            return true;
        }

        LiveDebugLog.Write(
            $"server_live_block_interaction_intent active=1 source=process_file path={blockInteractionIntentPath} " +
            $"frame={plan.FrameIndex} commands={plan.CommandCount} break={plan.BreakCommandCount} place={plan.PlaceCommandCount}");

        int submitResult;
        if (NativeHostPolicyLibrary.GetStartupPolicy().LiveProcessStream)
        {
            // The local client's camera is a presentation sample, not edit authority.
            var authoritativePlayer = gameModule.SnapshotPlayer();
            for (var index = 0; index < plan.CommandCount; ++index)
            {
                commands[index].X = authoritativePlayer.X;
                commands[index].Y = authoritativePlayer.Y;
                commands[index].Z = authoritativePlayer.Z;
            }
        }
        fixed (HostCommand* commandPointer = commands)
        {
            submitResult = gameModule.SubmitClientCommands(commandPointer, plan.CommandCount);
        }
        LiveDebugLog.Write($"server_live_block_interaction_submit result={submitResult} commands={plan.CommandCount}");

        // Capacity (-1) is retryable. Accepted and rejected commands are consumed once.
        if (submitResult != -1)
        {
            NativeBlockStoreLibrary.BlockInteractionFrameTrackerNoteSubmitted(
                BlockInteractionFrameTracker,
                plan.FrameIndex);
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
