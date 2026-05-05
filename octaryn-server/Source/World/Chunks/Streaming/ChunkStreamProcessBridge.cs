using System.Runtime.InteropServices;
using System.Text.Json;
using Octaryn.Server.Modules;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;
using Octaryn.Server.World.Chunks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static unsafe class ChunkStreamProcessBridge
{
    private const string IntentPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH";
    private const string StreamPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_STREAM_PATH";
    private const string PlayerInputIntentPathEnvironmentVariable = "OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH";
    private const string BlockInteractionIntentPathEnvironmentVariable = "OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH";
    private const string WorldTimeIntentPathEnvironmentVariable = "OCTARYN_SERVER_WORLD_TIME_INTENT_PATH";
    private const string MetadataOnlyEnvironmentVariable = "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY";
    private static readonly JsonSerializerOptions s_jsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };
    private static readonly IntPtr s_streamWriteTracker = NativeBlockStoreLibrary.ChunkStreamWriteTrackerCreate();
    private static ulong s_lastBlockInteractionFrame;

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

        var intentReadResult = TryReadChunkViewIntent(intentPath, allowMissingIntent, out var intent);
        if (intentReadResult > 0)
        {
            return 0;
        }
        if (intentReadResult < 0)
        {
            return -1;
        }

        if (!TryReadPlayerInputIntent(allowMissingIntent, out var frame, out var shouldTick))
        {
            return -1;
        }

        ApplyWorldTimeIntentIfRequested(gameModule);
        var metadataOnly = IsEnabled(MetadataOnlyEnvironmentVariable);

        if (!ApplyBlockInteractionIntentIfRequested(gameModule, allowMissingIntent, out var submittedBlockCommands))
        {
            return -1;
        }

        if (shouldTick || submittedBlockCommands)
        {
            if (!shouldTick)
            {
                frame = CreateProcessFrame();
            }

            if (metadataOnly)
            {
                gameModule.TickHostOnly(in frame);
            }
            else
            {
                gameModule.Tick(in frame);
            }
        }

        LiveDebugLog.Write($"server_live_chunk_view_intent source=process_file path={intentPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
        var writeDecision = NativeBlockStoreLibrary.ChunkStreamWriteTrackerDecide(
            StreamWriteTracker,
            metadataOnly ? 1u : 0u,
            submittedBlockCommands ? 1u : 0u,
            intent.CenterChunkX,
            intent.CenterChunkZ,
            intent.Radius,
            intent.HasPreviousWindow,
            intent.PreviousCenterChunkX,
            intent.PreviousCenterChunkZ,
            intent.PreviousRadius);
        var usePreviousWindow = writeDecision.UsePreviousWindow != 0;
        if (writeDecision.ShouldWrite == 0)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=1 skipped=1 reason=unchanged_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
            return 0;
        }

        var worldTime = gameModule.SnapshotWorldTime();
        var player = gameModule.SnapshotPlayer();
        var writeResult = gameModule.WriteChunkStreamSnapshotFile(
            streamPath,
            intent.Epoch,
            intent.CenterChunkX,
            intent.CenterChunkZ,
            intent.Radius,
            usePreviousWindow,
            intent.PreviousCenterChunkX,
            intent.PreviousCenterChunkZ,
            intent.PreviousRadius,
            metadataOnly,
            worldTime,
            player);
        NativeBlockStoreLibrary.ChunkStreamWriteTrackerNoteWritten(
            StreamWriteTracker,
            intent.CenterChunkX,
            intent.CenterChunkZ,
            intent.Radius);

        LiveDebugLog.Write($"server_live_chunk_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} load={writeResult.LoadCount} preserve={writeResult.PreserveCount} unload={writeResult.UnloadCount}");
        LiveDebugLog.Write($"server_live_chunk_stream active=1 source=process_file path={streamPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} columns={writeResult.Counts.ColumnCount} blocks={writeResult.Counts.BlockCount} metadata_only={(metadataOnly ? 1 : 0)} world_time_day_fraction={worldTime.DayFraction:F6}");
        return 0;
    }

    private static IntPtr StreamWriteTracker =>
        s_streamWriteTracker != IntPtr.Zero
            ? s_streamWriteTracker
            : throw new InvalidOperationException("Native chunk stream write tracker allocation failed.");

    private static int TryReadChunkViewIntent(string path, bool allowTransientInvalid, out NativeChunkViewIntent intent)
    {
        intent = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var nativeIntent = stackalloc NativeChunkViewIntent[1];
            var result = NativeBlockStoreLibrary.ChunkStreamReadViewIntent((byte*)pathPointer, nativeIntent);
            intent = nativeIntent[0];
            switch (result)
            {
                case 0:
                    return 0;
                case 1:
                    LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=missing_intent path={path}");
                    return allowTransientInvalid ? 1 : -1;
                case -2:
                    LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=intent_read_retry path={path}");
                    return allowTransientInvalid ? 1 : -1;
                case -3:
                    LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=partial_intent path={path}");
                    return allowTransientInvalid ? 1 : -1;
                case -4:
                    LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=unsupported_intent path={path}");
                    return -1;
                default:
                    LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=intent_read_failed path={path}");
                    return -1;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
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

        BlockInteractionIntentFile? intent;
        try
        {
            intent = JsonSerializer.Deserialize<BlockInteractionIntentFile>(
                File.ReadAllText(blockInteractionIntentPath),
                s_jsonOptions);
        }
        catch (JsonException)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=partial_intent path={blockInteractionIntentPath}");
            return allowTransientInvalid;
        }
        catch (IOException)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=intent_read_retry path={blockInteractionIntentPath}");
            return allowTransientInvalid;
        }

        if (intent is null || !intent.IsSupported)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=unsupported_intent path={blockInteractionIntentPath}");
            return false;
        }

        if (intent.FrameIndex <= s_lastBlockInteractionFrame)
        {
            LiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=duplicate_frame path={blockInteractionIntentPath} frame={intent.FrameIndex}");
            return true;
        }

        var commands = intent.Commands.Select(command => command.ToHostCommand()).ToArray();
        var breakCommands = commands.Count(command => command.D == BlockId.Air.Value);
        var placeCommands = commands.Length - breakCommands;
        LiveDebugLog.Write(
            $"server_live_block_interaction_intent active=1 source=process_file path={blockInteractionIntentPath} " +
            $"frame={intent.FrameIndex} commands={commands.Length} break={breakCommands} place={placeCommands}");

        var result = gameModule.SubmitClientCommands(commands);
        LiveDebugLog.Write($"server_live_block_interaction_submit result={result} commands={commands.Length}");
        if (result != 0)
        {
            return false;
        }

        s_lastBlockInteractionFrame = intent.FrameIndex;
        submittedBlockCommands = commands.Length > 0;
        return true;
    }

    private static HostFrameSnapshot CreateProcessFrame()
    {
        return new HostFrameSnapshot(
            new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
            new HostFrameTimingSnapshot(
                HostFrameTimingSnapshot.VersionValue,
                HostFrameTimingSnapshot.SizeValue,
                frameIndex: 1,
                deltaSeconds: 1.0 / 60.0));
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
