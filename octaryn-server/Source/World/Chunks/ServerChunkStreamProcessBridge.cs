using System.Text.Json;
using Octaryn.Shared.World;
using Octaryn.Server.World.Chunks;

namespace Octaryn.Server;

internal static class ServerChunkStreamProcessBridge
{
    private const string IntentPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH";
    private const string StreamPathEnvironmentVariable = "OCTARYN_SERVER_CHUNK_STREAM_PATH";
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
        var file = ServerChunkStreamSnapshotFile.From(intent.Epoch, stream);

        var directory = Path.GetDirectoryName(streamPath);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        File.WriteAllText(streamPath, JsonSerializer.Serialize(file, s_jsonOptions));
        ServerLiveDebugLog.Write($"server_live_chunk_window epoch={stream.Window.Epoch} center=({stream.CenterChunkX},{stream.CenterChunkZ}) radius={stream.Radius} load={stream.Window.LoadCount} preserve={stream.Window.PreserveCount} unload={stream.Window.UnloadCount}");
        ServerLiveDebugLog.Write($"server_live_chunk_stream active=1 source=process_file path={streamPath} epoch={intent.Epoch} center=({stream.CenterChunkX},{stream.CenterChunkZ}) radius={stream.Radius} columns={stream.Columns.Count} blocks={stream.Blocks.Count}");
        return 0;
    }
}
