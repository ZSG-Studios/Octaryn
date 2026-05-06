using System.Runtime.InteropServices;
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
    private const uint WorldTimeIntentReasonMissingIntent = 1;
    private const uint WorldTimeIntentReasonInvalidIntent = 2;
    private const uint WorldTimeIntentReasonUnsupportedIntent = 3;
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

        if (!TryReadPlayerInputIntent(allowMissingIntent, out var frame, out var hasPlayerInput))
        {
            return -1;
        }

        ApplyWorldTimeIntentIfRequested(gameModule);
        var metadataOnly = NativeHostPolicyLibrary.EnvironmentEnabled(MetadataOnlyEnvironmentVariable);

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
                0,
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
            LogChunkStreamPlanStopReason(intentPath, writePlan);
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

    private static void ApplyWorldTimeIntentIfRequested(ModuleActivator gameModule)
    {
        var path = Environment.GetEnvironmentVariable(WorldTimeIntentPathEnvironmentVariable);
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
        if (plan.Reason == WorldTimeIntentReasonMissingIntent)
        {
            return;
        }

        var reason = plan.Reason switch
        {
            WorldTimeIntentReasonUnsupportedIntent => "unsupported_intent",
            WorldTimeIntentReasonInvalidIntent => "invalid_intent",
            _ => "invalid_intent",
        };
        LiveDebugLog.Write($"server_live_world_time_intent active=0 reason={reason} path={path}");
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

        var readResult = NativePlayerSimulation.ReadInputIntentFile(playerInputIntentPath, out var intent);
        if (NativePlayerSimulation.PlanInputIntent(readResult, allowTransientInvalid, intent, out var plan) != 0)
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=intent_read_failed path={playerInputIntentPath}");
            return false;
        }
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
        frame = intent.ToFrameSnapshot();
        shouldTick = true;
        return true;
    }

    private static void LogPlayerInputPlanStopReason(string path, NativeInputProcessPlan plan)
    {
        var reason = NativePlayerSimulation.InputProcessReasonName(plan.Reason);
        LiveDebugLog.Write($"server_live_player_input_intent active=0 reason={reason} path={path}");
    }

    private static bool ApplyBlockInteractionIntentIfRequested(ModuleActivator gameModule, bool allowTransientInvalid, out bool submittedBlockCommands)
    {
        submittedBlockCommands = false;
        var blockInteractionIntentPath = Environment.GetEnvironmentVariable(BlockInteractionIntentPathEnvironmentVariable);
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
        if (submitResult != 0)
        {
            return false;
        }

        NativeBlockStoreLibrary.BlockInteractionFrameTrackerNoteSubmitted(
            BlockInteractionFrameTracker,
            plan.FrameIndex);
        submittedBlockCommands = plan.CommandCount > 0;
        return true;
    }

    private static void LogBlockInteractionPlanStopReason(string path, NativeBlockInteractionProcessPlan plan)
    {
        var reason = NativeText(NativeBlockStoreLibrary.BlockInteractionProcessReasonName(plan.Reason));
        var frameSuffix = reason == "duplicate_frame" ? $" frame={plan.FrameIndex}" : string.Empty;
        LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason={reason} path={path}{frameSuffix}");
    }

    private static string NativeText(byte* value)
    {
        return Marshal.PtrToStringUTF8((IntPtr)value) ?? "intent_read_failed";
    }

}
