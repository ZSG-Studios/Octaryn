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
 private static readonly PlayerCommandQueue s_playerCommands = new();
 private static readonly BlockCommandAdmission s_blockAdmission = new();
 private static readonly Stopwatch s_liveTickClock = Stopwatch.StartNew();
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

        return HandleSessionPaths(
            gameModule,
            new SessionFilePaths(
                paths.ChunkViewIntentPath!,
                paths.ChunkStreamPath!,
                paths.PlayerInputIntentPath,
                paths.PlayerStateStreamPath,
                paths.BlockInteractionIntentPath,
                paths.WorldTimeIntentPath,
                paths.MetadataOnly),
            allowMissingIntent);
    }

    internal static int HandleSessionPaths(ModuleActivator gameModule, SessionFilePaths paths, bool allowMissingIntent = false)
    {
        if (gameModule.BlockReceiptSession is null && !string.IsNullOrWhiteSpace(paths.BlockInteractionIntent))
            gameModule.BeginBlockReceiptSession(Path.GetDirectoryName(Path.GetFullPath(paths.BlockInteractionIntent))!);
        var intentPath = paths.ChunkViewIntent;
        var streamPath = paths.ChunkStream;

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

        s_playerCommands.Read(paths.PlayerInputIntent);

 s_playerCommands.Accrue();
 var hasPlayerInput = s_playerCommands.TrySelect(s_sourceTick + 1, out var frame);
 gameModule.SetFluidRegion(intent.CenterChunkX, intent.CenterChunkZ, intent.Radius);
        ApplyWorldTimeIntentIfRequested(gameModule, paths.WorldTimeIntent);
        var metadataOnly = paths.MetadataOnly;

        if (!ApplyBlockInteractionIntentIfRequested(gameModule, paths.BlockInteractionIntent, allowMissingIntent, out var submittedBlockCommands))
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
        // A published pose acknowledges consumed input, never merely a read file.
 s_acknowledgedInputFrame = s_playerCommands.Acknowledged;
        var player = gameModule.SnapshotPlayer();
        var playerWorldTime = gameModule.SnapshotWorldTime();
        if (!TryWritePlayerStateStream(paths.PlayerStateStream, s_sourceTick, player,
            playerWorldTime.DayFraction, playerWorldTime.TotalWorldSeconds))
        {
            return -1;
        }
 return PublishSnapshot(gameModule, streamPath, intent, metadataOnly, submittedBlockCommands);
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
        gameModule.SetWorldTimeHourOffset(intent.HourOffset);
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
 $"\"simulationTick\":{s_sourceTick},\"simulationTime\":{s_sourceSeconds:R}," +
                $"\"acknowledgedInputFrame\":{s_acknowledgedInputFrame}," +
                $"\"worldTimeDayFraction\":{worldDayFraction:R},\"worldTimeTotalSeconds\":{worldTotalSeconds:R}," +
                $"\"playerX\":{player.X:R},\"playerY\":{player.Y:R},\"playerZ\":{player.Z:R}," +
                $"\"playerPitch\":{player.Pitch:R},\"playerYaw\":{player.Yaw:R}," +
                $"\"playerVelocityX\":{player.VelocityX:R},\"playerVelocityY\":{player.VelocityY:R}," +
                $"\"playerVelocityZ\":{player.VelocityZ:R},\"playerControlMode\":{player.ControlMode}," +
 $"\"playerOnGround\":{(player.IsOnGround ? 1 : 0)},\"jumpHeld\":{(player.JumpHeld ? 1 : 0)}}}");
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

        var admission = s_blockAdmission.Prepare(gameModule, blockInteractionIntentPath, plan.FrameIndex,
            commands.AsSpan(0, checked((int)plan.CommandCount)), out var admitted);
        if (admission == BlockCommandAdmission.Decision.Wait) return true;
        var submitResult = 0;
        if (admission == BlockCommandAdmission.Decision.Submit)
        {
            // Reach is always measured from the authoritative body, including remote play.
            var authoritativePlayer = gameModule.SnapshotPlayer();
            for (var index = 0; index < admitted.Length; ++index)
            {
                admitted[index].X = authoritativePlayer.X;
                admitted[index].Y = authoritativePlayer.Y;
                admitted[index].Z = authoritativePlayer.Z;
            }
            fixed (HostCommand* commandPointer = admitted)
            {
                submitResult = gameModule.SubmitClientCommands(commandPointer, (uint)admitted.Length);
            }
            if (submitResult != -1) s_blockAdmission.Submitted();
        }
        LiveDebugLog.Write($"server_live_block_interaction_submit result={submitResult} commands={plan.CommandCount}");

        // Capacity (-1) is retryable. Accepted and rejected commands are consumed once.
        if (submitResult != -1 && s_blockAdmission.IsComplete)
        {
            NativeBlockStoreLibrary.BlockInteractionFrameTrackerNoteSubmitted(
                BlockInteractionFrameTracker,
                plan.FrameIndex);
            TryClearSubmittedBlockInteractionIntent(blockInteractionIntentPath);
        }
        submittedBlockCommands = submitResult == 0 && admitted.Length > 0;
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
