using System.Text.Json;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static class ServerChunkStreamProcessBridge
{
    private const string IntentPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH";
    private const string StreamPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_STREAM_PATH";
    private const string PlayerInputIntentPathEnvironmentVariable = "OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH";
    private const string BlockInteractionIntentPathEnvironmentVariable = "OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH";
    private static readonly JsonSerializerOptions s_jsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    public static int HandleIfRequested(ServerModuleActivator basegame)
    {
        var intentPath = Environment.GetEnvironmentVariable(IntentPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(intentPath))
        {
            return 0;
        }

        var streamPath = Environment.GetEnvironmentVariable(StreamPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(streamPath))
        {
            ServerLiveDebugLog.Write("server_live_chunk_stream active=0 reason=missing_stream_path");
            return -1;
        }

        if (!File.Exists(intentPath))
        {
            ServerLiveDebugLog.Write($"server_live_chunk_stream active=0 reason=missing_intent path={intentPath}");
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
            ServerLiveDebugLog.Write($"server_live_chunk_stream active=0 reason=invalid_intent path={intentPath}");
            return -1;
        }

        if (intent is null ||
            intent.Version != 1 ||
            intent.Radius > ChunkColumnStreamingLimits.MaxRequestRadius)
        {
            ServerLiveDebugLog.Write($"server_live_chunk_stream active=0 reason=unsupported_intent path={intentPath}");
            return -1;
        }

        if (!TryReadPlayerInputIntent(out var frame, out var shouldTick))
        {
            return -1;
        }

        if (!ApplyBlockInteractionIntentIfRequested(basegame, out var submittedBlockCommands))
        {
            return -1;
        }

        if (shouldTick || submittedBlockCommands)
        {
            if (!shouldTick)
            {
                frame = CreateProcessFrame();
            }

            basegame.Tick(in frame);
        }

        ServerLiveDebugLog.Write($"server_live_chunk_view_intent source=process_file path={intentPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
        var stream = basegame.CaptureChunkColumns(
            intent.CenterChunkX,
            intent.CenterChunkZ,
            intent.Radius,
            intent.Epoch,
            intent.HasPreviousWindow,
            intent.PreviousCenterChunkX,
            intent.PreviousCenterChunkZ,
            intent.PreviousRadius);
        var file = ServerChunkStreamSnapshotFile.From(
            intent.Epoch,
            stream,
            basegame.SnapshotWorldTime(),
            basegame.SnapshotPlayerState());

        var directory = Path.GetDirectoryName(streamPath);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        File.WriteAllText(streamPath, JsonSerializer.Serialize(file, s_jsonOptions));
        ServerLiveDebugLog.Write($"server_live_chunk_window epoch={stream.Window.Epoch} center=({stream.CenterChunkX},{stream.CenterChunkZ}) radius={stream.Radius} load={stream.Window.LoadCount} preserve={stream.Window.PreserveCount} unload={stream.Window.UnloadCount}");
        ServerLiveDebugLog.Write($"server_live_chunk_stream active=1 source=process_file path={streamPath} epoch={intent.Epoch} center=({stream.CenterChunkX},{stream.CenterChunkZ}) radius={stream.Radius} columns={stream.Columns.Count} blocks={stream.Blocks.Count} world_time_day_fraction={file.WorldTimeDayFraction:F6}");
        return 0;
    }

    private static bool TryReadPlayerInputIntent(out HostFrameSnapshot frame, out bool shouldTick)
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
            ServerLiveDebugLog.Write($"server_live_player_input_intent active=0 reason=missing_intent path={playerInputIntentPath}");
            return false;
        }

        ServerPlayerInputIntentFile? intent;
        try
        {
            intent = JsonSerializer.Deserialize<ServerPlayerInputIntentFile>(
                File.ReadAllText(playerInputIntentPath),
                s_jsonOptions);
        }
        catch (JsonException)
        {
            ServerLiveDebugLog.Write($"server_live_player_input_intent active=0 reason=invalid_intent path={playerInputIntentPath}");
            return false;
        }

        if (intent is null || !intent.IsSupported)
        {
            ServerLiveDebugLog.Write($"server_live_player_input_intent active=0 reason=unsupported_intent path={playerInputIntentPath}");
            return false;
        }

        ServerLiveDebugLog.Write(
            $"server_live_player_input_intent active=1 source=process_file path={playerInputIntentPath} " +
            $"frame={intent.FrameIndex} dt={intent.DeltaSeconds:F6} flags={intent.Flags} controller={intent.Controller} " +
            $"move=({intent.MoveX:F3},{intent.MoveY:F3},{intent.MoveZ:F3}) " +
            $"camera=({intent.CameraX:F3},{intent.CameraY:F3},{intent.CameraZ:F3},{intent.CameraPitch:F6},{intent.CameraYaw:F6})");
        frame = intent.ToFrameSnapshot();
        shouldTick = true;
        return true;
    }

    private static bool ApplyBlockInteractionIntentIfRequested(ServerModuleActivator basegame, out bool submittedBlockCommands)
    {
        submittedBlockCommands = false;
        var blockInteractionIntentPath = Environment.GetEnvironmentVariable(BlockInteractionIntentPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(blockInteractionIntentPath))
        {
            return true;
        }

        if (!File.Exists(blockInteractionIntentPath))
        {
            ServerLiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=missing_intent path={blockInteractionIntentPath}");
            return false;
        }

        ServerBlockInteractionIntentFile? intent;
        try
        {
            intent = JsonSerializer.Deserialize<ServerBlockInteractionIntentFile>(
                File.ReadAllText(blockInteractionIntentPath),
                s_jsonOptions);
        }
        catch (JsonException)
        {
            ServerLiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=invalid_intent path={blockInteractionIntentPath}");
            return false;
        }

        if (intent is null || !intent.IsSupported)
        {
            ServerLiveDebugLog.Write($"server_live_block_interaction_intent active=0 reason=unsupported_intent path={blockInteractionIntentPath}");
            return false;
        }

        var commands = intent.Commands.Select(command => command.ToHostCommand()).ToArray();
        var breakCommands = commands.Count(command => command.D == BlockId.Air.Value);
        var placeCommands = commands.Length - breakCommands;
        ServerLiveDebugLog.Write(
            $"server_live_block_interaction_intent active=1 source=process_file path={blockInteractionIntentPath} " +
            $"frame={intent.FrameIndex} commands={commands.Length} break={breakCommands} place={placeCommands}");

        var result = basegame.SubmitClientCommands(commands);
        ServerLiveDebugLog.Write($"server_live_block_interaction_submit result={result} commands={commands.Length}");
        if (result != 0)
        {
            return false;
        }

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
}
