using System.Text.Json;
using Octaryn.Server.Modules;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static class ChunkStreamProcessBridge
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
    private static ChunkStreamWriteSignature? s_lastWrittenStream;
    private static readonly HashSet<ChunkStreamWriteSignature> s_writtenBlockStreams = [];
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

        ClientChunkViewIntentFile? intent;
        try
        {
            intent = JsonSerializer.Deserialize<ClientChunkViewIntentFile>(
                File.ReadAllText(intentPath),
                s_jsonOptions);
        }
        catch (JsonException)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=partial_intent path={intentPath}");
            return allowMissingIntent ? 0 : -1;
        }
        catch (IOException)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=intent_read_retry path={intentPath}");
            return allowMissingIntent ? 0 : -1;
        }

        if (intent is null ||
            intent.Version != 1 ||
            intent.Radius > ChunkColumnStreamingLimits.MaxRequestRadius)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=0 reason=unsupported_intent path={intentPath}");
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
        var requestedWindow = new ChunkStreamWriteSignature(
            intent.CenterChunkX,
            intent.CenterChunkZ,
            intent.Radius);
        var previousWindow = new ChunkStreamWriteSignature(
            intent.PreviousCenterChunkX,
            intent.PreviousCenterChunkZ,
            intent.PreviousRadius);
        var hasTrustedPreviousWindow =
            s_writtenBlockStreams.Contains(requestedWindow) ||
            (intent.HasPreviousWindow && s_writtenBlockStreams.Contains(previousWindow));
        var stream = gameModule.CaptureChunkColumns(
            intent.CenterChunkX,
            intent.CenterChunkZ,
            intent.Radius,
            intent.Epoch,
            metadataOnly ? intent.HasPreviousWindow && hasTrustedPreviousWindow : intent.HasPreviousWindow,
            intent.PreviousCenterChunkX,
            intent.PreviousCenterChunkZ,
            intent.PreviousRadius,
            metadataOnly);
        var file = ChunkStreamSnapshotFile.From(
            intent.Epoch,
            stream,
            gameModule.SnapshotWorldTime(),
            gameModule.SnapshotPlayer());
        if (ShouldSkipStreamWrite(metadataOnly, submittedBlockCommands, stream))
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=1 skipped=1 reason=unchanged_window epoch={intent.Epoch} center=({stream.CenterChunkX},{stream.CenterChunkZ}) radius={stream.Radius} columns={stream.Columns.Count}");
            return 0;
        }

        var directory = Path.GetDirectoryName(streamPath);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        WriteChunkStreamFile(streamPath, file);
        var writtenWindow = ChunkStreamWriteSignature.From(stream);
        s_lastWrittenStream = writtenWindow;
        if (stream.Blocks.Count != 0)
        {
            s_writtenBlockStreams.Add(writtenWindow);
        }

        LiveDebugLog.Write($"server_live_chunk_window epoch={stream.Window.Epoch} center=({stream.CenterChunkX},{stream.CenterChunkZ}) radius={stream.Radius} load={stream.Window.LoadCount} preserve={stream.Window.PreserveCount} unload={stream.Window.UnloadCount}");
        LiveDebugLog.Write($"server_live_chunk_stream active=1 source=process_file path={streamPath} epoch={intent.Epoch} center=({stream.CenterChunkX},{stream.CenterChunkZ}) radius={stream.Radius} columns={stream.Columns.Count} blocks={stream.Blocks.Count} metadata_only={(metadataOnly ? 1 : 0)} world_time_day_fraction={file.WorldTimeDayFraction:F6}");
        return 0;
    }

    private static bool ShouldSkipStreamWrite(
        bool metadataOnly,
        bool submittedBlockCommands,
        ChunkColumnStream stream)
    {
        var streamSignature = ChunkStreamWriteSignature.From(stream);
        if (!metadataOnly ||
            submittedBlockCommands ||
            stream.Blocks.Count != 0 ||
            stream.Window.LoadCount != 0 ||
            stream.Window.UnloadCount != 0 ||
            !s_writtenBlockStreams.Contains(streamSignature) ||
            s_lastWrittenStream is not { } lastWrittenStream)
        {
            return false;
        }

        return lastWrittenStream == streamSignature;
    }

    private static void WriteChunkStreamFile(string streamPath, ChunkStreamSnapshotFile file)
    {
        var temporaryPath = $"{streamPath}.tmp";
        File.WriteAllText(temporaryPath, JsonSerializer.Serialize(file, s_jsonOptions));
        File.Move(temporaryPath, streamPath, overwrite: true);
    }

    private readonly record struct ChunkStreamWriteSignature(
        int CenterChunkX,
        int CenterChunkZ,
        uint Radius)
    {
        public static ChunkStreamWriteSignature From(ChunkColumnStream stream)
        {
            return new ChunkStreamWriteSignature(
                stream.CenterChunkX,
                stream.CenterChunkZ,
                stream.Radius);
        }
    }

    private static void ApplyWorldTimeIntentIfRequested(ModuleActivator gameModule)
    {
        var path = Environment.GetEnvironmentVariable(WorldTimeIntentPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
        {
            return;
        }

        WorldTimeIntentFile? intent;
        try
        {
            intent = JsonSerializer.Deserialize<WorldTimeIntentFile>(
                File.ReadAllText(path),
                s_jsonOptions);
        }
        catch (JsonException)
        {
            LiveDebugLog.Write($"server_live_world_time_intent active=0 reason=invalid_intent path={path}");
            return;
        }

        if (intent is null || !intent.IsSupported)
        {
            LiveDebugLog.Write($"server_live_world_time_intent active=0 reason=unsupported_intent path={path}");
            return;
        }

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

        PlayerInputIntentFile? intent;
        try
        {
            intent = JsonSerializer.Deserialize<PlayerInputIntentFile>(
                File.ReadAllText(playerInputIntentPath),
                s_jsonOptions);
        }
        catch (JsonException)
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=partial_intent path={playerInputIntentPath}");
            return allowTransientInvalid;
        }
        catch (IOException)
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=intent_read_retry path={playerInputIntentPath}");
            return allowTransientInvalid;
        }

        if (intent is null || !intent.IsSupported)
        {
            LiveDebugLog.Write($"server_live_player_input_intent active=0 reason=unsupported_intent path={playerInputIntentPath}");
            return false;
        }

        LiveDebugLog.Write(
            $"server_live_player_input_intent active=1 source=process_file path={playerInputIntentPath} " +
            $"frame={intent.FrameIndex} dt={intent.DeltaSeconds:F6} flags={intent.Flags} controller={intent.Controller} " +
            $"move=({intent.MoveX:F3},{intent.MoveY:F3},{intent.MoveZ:F3}) " +
            $"camera=({intent.CameraX:F3},{intent.CameraY:F3},{intent.CameraZ:F3},{intent.CameraPitch:F6},{intent.CameraYaw:F6})");
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
