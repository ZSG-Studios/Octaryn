using System.Runtime.InteropServices;
using Octaryn.Server.Modules;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Chunks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static unsafe class ChunkStreamProcessBridge
{
    private const uint ProcessWriteReasonMissingIntent = 1;
    private const uint ProcessWriteReasonIntentReadRetry = 2;
    private const uint ProcessWriteReasonPartialIntent = 3;
    private const uint ProcessWriteReasonUnsupportedIntent = 4;
    private const uint ProcessWriteReasonIntentReadFailed = 5;
    private const string IntentPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH";
    private const string StreamPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_STREAM_PATH";
    private const string PlayerInputIntentPathEnvironmentVariable = "OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH";
    private const string BlockInteractionIntentPathEnvironmentVariable = "OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH";
    private const string WorldTimeIntentPathEnvironmentVariable = "OCTARYN_SERVER_WORLD_TIME_INTENT_PATH";
    private const string MetadataOnlyEnvironmentVariable = "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY";
    private static readonly IntPtr s_streamWriteTracker = NativeBlockStoreLibrary.ChunkStreamWriteTrackerCreate();
    private static readonly IntPtr s_blockInteractionFrameTracker =
        NativeBlockStoreLibrary.BlockInteractionFrameTrackerCreate();

    public static int HandleIfRequested(ModuleActivator gameModule, bool allowMissingIntent = false)
    {
        var intentPath = Environment.GetEnvironmentVariable(IntentPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(intentPath))
        {
            return 0;
        }

        var streamPath = Environment.GetEnvironmentVariable(StreamPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(streamPath))
        {
            LiveDebugLog.Write("server_live_chunk_stream active=0 reason=missing_stream_path");
            return -1;
        }

        if (!File.Exists(intentPath))
        {
            if (allowMissingIntent)
            {
                LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=waiting_for_intent path={intentPath}");
                return 0;
            }

            LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=missing_intent path={intentPath}");
            return -1;
        }

        var intentReadResult = ReadChunkViewIntent(intentPath, out var intent);
        if (intentReadResult != 0)
        {
            var intentStopPlan = default(NativeChunkStreamProcessWritePlan);
            if (NativeBlockStoreLibrary.ChunkStreamPlanProcessWrite(
                    StreamWriteTracker,
                    intentReadResult,
                    allowMissingIntent ? 1u : 0u,
                    &intent,
                    0u,
                    0u,
                    &intentStopPlan) != 0)
            {
                LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=intent_read_failed path={intentPath}");
                return -1;
            }

            LogChunkStreamPlanStopReason(intentPath, intentStopPlan.Reason);
            return intentStopPlan.HandleResult;
        }

        if (!TryReadPlayerInputIntent(allowMissingIntent, out var frame, out var hasPlayerInput))
        {
            return -1;
        }

        ApplyWorldTimeIntentIfRequested(gameModule);
        var metadataOnly = IsEnabled(MetadataOnlyEnvironmentVariable);

        if (!ApplyBlockInteractionIntentIfRequested(gameModule, allowMissingIntent, out var submittedBlockCommands))
        {
            return -1;
        }

        var tickDecision = NativeBlockStoreLibrary.ChunkStreamDecideProcessTick(
            hasPlayerInput ? 1u : 0u,
            submittedBlockCommands ? 1u : 0u,
            metadataOnly ? 1u : 0u);
        if (tickDecision.ShouldTick != 0)
        {
            if (tickDecision.UseDefaultFrame != 0 &&
                NativeBlockStoreLibrary.ChunkStreamCreateProcessFrame(&frame) != 0)
            {
                return -1;
            }

            if (tickDecision.UseHostOnlyTick != 0)
            {
                gameModule.TickHostOnly(in frame);
            }
            else
            {
                gameModule.Tick(in frame);
            }
        }

        var writePlan = default(NativeChunkStreamProcessWritePlan);
        if (NativeBlockStoreLibrary.ChunkStreamPlanProcessWrite(
                StreamWriteTracker,
                intentReadResult,
                allowMissingIntent ? 1u : 0u,
                &intent,
                metadataOnly ? 1u : 0u,
                submittedBlockCommands ? 1u : 0u,
                &writePlan) != 0)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=intent_read_failed path={intentPath}");
            return -1;
        }
        if (writePlan.ShouldContinue == 0)
        {
            LogChunkStreamPlanStopReason(intentPath, writePlan.Reason);
            return writePlan.HandleResult;
        }

        LiveDebugLog.Write($"server_live_chunk_view_intent source=process_file path={intentPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
        var usePreviousWindow = writePlan.UsePreviousWindow != 0;
        if (writePlan.ShouldWrite == 0)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=1 skipped=1 reason=unchanged_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
            return 0;
        }

        var worldTime = gameModule.SnapshotWorldTime();
        var player = gameModule.SnapshotPlayer();
        var writeResult = gameModule.WriteChunkStreamSnapshotFile(
            streamPath,
            intent.Epoch,
            writePlan.CenterChunkX,
            writePlan.CenterChunkZ,
            writePlan.Radius,
            usePreviousWindow,
            intent.PreviousCenterChunkX,
            intent.PreviousCenterChunkZ,
            intent.PreviousRadius,
            metadataOnly,
            worldTime,
            player);
        NativeBlockStoreLibrary.ChunkStreamProcessWritePlanNoteWritten(
            StreamWriteTracker,
            &writePlan);

        LiveDebugLog.Write($"server_live_chunk_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} load={writeResult.LoadCount} preserve={writeResult.PreserveCount} unload={writeResult.UnloadCount}");
        LiveDebugLog.Write($"server_live_chunk_stream active=1 source=process_file path={streamPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} columns={writeResult.Counts.ColumnCount} blocks={writeResult.Counts.BlockCount} metadata_only={(metadataOnly ? 1 : 0)} world_time_day_fraction={worldTime.DayFraction:F6}");
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

    private static int ReadChunkViewIntent(string path, out NativeChunkViewIntent intent)
    {
        intent = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var nativeIntent = stackalloc NativeChunkViewIntent[1];
            var result = NativeBlockStoreLibrary.ChunkStreamReadViewIntent((byte*)pathPointer, nativeIntent);
            intent = nativeIntent[0];
            return result;
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    private static void LogChunkStreamPlanStopReason(string path, uint reason)
    {
        var text = reason switch
        {
            ProcessWriteReasonMissingIntent => "missing_intent",
            ProcessWriteReasonIntentReadRetry => "intent_read_retry",
            ProcessWriteReasonPartialIntent => "partial_intent",
            ProcessWriteReasonUnsupportedIntent => "unsupported_intent",
            ProcessWriteReasonIntentReadFailed => "intent_read_failed",
            _ => "intent_read_failed",
        };
        LiveDebugLog.Write($"server_live_chunk_stream active=0 reason={text} path={path}");
    }

    private static void ApplyWorldTimeIntentIfRequested(ModuleActivator gameModule)
    {
        var path = Environment.GetEnvironmentVariable(WorldTimeIntentPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
        {
            return;
        }

        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        var nativeIntent = stackalloc NativeWorldTimeIntent[1];
        int readResult;
        try
        {
            readResult = NativeWorldTimeLibrary.ReadIntentFile((byte*)pathPointer, nativeIntent);
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
        if (readResult != 0)
        {
            var reason = readResult == -4 ? "unsupported_intent" : "invalid_intent";
            LiveDebugLog.Write($"server_live_world_time_intent active=0 reason={reason} path={path}");
            return;
        }

        var intent = nativeIntent[0];
        gameModule.SetWorldTimeSpeedMultiplier(intent.SpeedMultiplier);
        LiveDebugLog.Write($"server_live_world_time_intent active=1 source=process_file path={path} speed_index={intent.SpeedIndex} speed_multiplier={intent.SpeedMultiplier:F3}");
    }

    private static bool TryReadPlayerInputIntent(bool allowTransientInvalid, out HostFrameSnapshot frame, out bool shouldTick)
    {
        frame = default;
        shouldTick = false;
        var playerInputIntentPath = Environment.GetEnvironmentVariable(PlayerInputIntentPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(playerInputIntentPath))
        {
            return true;
        }

        if (!File.Exists(playerInputIntentPath))
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=waiting_for_intent path={playerInputIntentPath}");
            return true;
        }

        var readResult = NativePlayerSimulation.ReadInputIntentFile(playerInputIntentPath, out var intent);
        if (readResult == -2)
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=intent_read_retry path={playerInputIntentPath}");
            return allowTransientInvalid;
        }
        if (readResult == -3)
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=partial_intent path={playerInputIntentPath}");
            return allowTransientInvalid;
        }
        if (readResult != 0)
        {
            var reason = readResult == -4 ? "unsupported_intent" : "intent_read_failed";
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason={reason} path={playerInputIntentPath}");
            return false;
        }

        LiveDebugLog.Write(
            $"server_live_player_input_intent active=1 source=process_file path={playerInputIntentPath} " +
            $"frame={intent.FrameIndex} dt={intent.DeltaSeconds:F6} flags={intent.Input.Flags} controller={intent.Input.Controller} " +
            $"move=({intent.Input.MoveX:F3},{intent.Input.MoveY:F3},{intent.Input.MoveZ:F3}) " +
            $"camera=({intent.Input.CameraX:F3},{intent.Input.CameraY:F3},{intent.Input.CameraZ:F3},{intent.Input.CameraPitch:F6},{intent.Input.CameraYaw:F6})");
        frame = intent.ToFrameSnapshot();
        shouldTick = true;
        return true;
    }

    private static bool ApplyBlockInteractionIntentIfRequested(ModuleActivator gameModule, bool allowTransientInvalid, out bool submittedBlockCommands)
    {
        submittedBlockCommands = false;
        var blockInteractionIntentPath = Environment.GetEnvironmentVariable(BlockInteractionIntentPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(blockInteractionIntentPath))
        {
            return true;
        }

        if (!File.Exists(blockInteractionIntentPath))
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=waiting_for_intent path={blockInteractionIntentPath}");
            return true;
        }

        var commands = new HostCommand[ClientBlockCommandQueue.MaxPendingCommands];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(blockInteractionIntentPath);
        var intent = default(NativeBlockInteractionIntentResult);
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
        if (readResult == -2)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=intent_read_retry path={blockInteractionIntentPath}");
            return allowTransientInvalid;
        }
        if (readResult == -3)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=partial_intent path={blockInteractionIntentPath}");
            return allowTransientInvalid;
        }
        if (readResult != 0)
        {
            var reason = readResult == -4 ? "unsupported_intent" : "intent_read_failed";
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason={reason} path={blockInteractionIntentPath}");
            return false;
        }

        var frameDecision = NativeBlockStoreLibrary.BlockInteractionFrameTrackerDecide(
            BlockInteractionFrameTracker,
            intent.FrameIndex);
        if (frameDecision.ShouldSubmit == 0)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=duplicate_frame path={blockInteractionIntentPath} frame={intent.FrameIndex}");
            return true;
        }

        LiveDebugLog.Write(
            $"server_live_block_interaction_intent active=1 source=process_file path={blockInteractionIntentPath} " +
            $"frame={intent.FrameIndex} commands={intent.CommandCount} break={intent.BreakCommandCount} place={intent.PlaceCommandCount}");

        int submitResult;
        fixed (HostCommand* commandPointer = commands)
        {
            submitResult = gameModule.SubmitClientCommands(commandPointer, intent.CommandCount);
        }
        LiveDebugLog.Write($"server_live_block_interaction_submit result={submitResult} commands={intent.CommandCount}");
        if (submitResult != 0)
        {
            return false;
        }

        NativeBlockStoreLibrary.BlockInteractionFrameTrackerNoteSubmitted(
            BlockInteractionFrameTracker,
            intent.FrameIndex);
        submittedBlockCommands = intent.CommandCount > 0;
        return true;
    }

    private static bool IsEnabled(string name)
    {
        var value = Environment.GetEnvironmentVariable(name);
        return !string.IsNullOrWhiteSpace(value) &&
            (value == "1" ||
             value.Equals("true", StringComparison.OrdinalIgnoreCase) ||
             value.Equals("yes", StringComparison.OrdinalIgnoreCase));
    }
}
