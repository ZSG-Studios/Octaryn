using System.Text.Json;
using Octaryn.Server.Modules;

namespace Octaryn.Server;

internal static unsafe partial class ChunkStreamProcessBridge
{
    private static ulong s_uiActionSequence;
    private static ulong s_uiActionEpoch;
    private static string? s_uiActionAckPayload;

    private static void ReadUiActionIntent(ModuleActivator gameModule, string? sessionPath = null)
    {
        var path = !string.IsNullOrWhiteSpace(sessionPath) ? sessionPath
            : Environment.GetEnvironmentVariable("OCTARYN_SERVER_UI_ACTION_INTENT_PATH");
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path)) return;
        try
        {
            if (new FileInfo(path).Length > 65536) return;
            AcceptUiActionIntent(gameModule, File.ReadAllBytes(path), path);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException) { }
    }

    internal static bool AcceptUiActionIntent(ModuleActivator gameModule, ReadOnlyMemory<byte> payload, string? path = null)
    {
        if (payload.Length > 65536) return true;
        try
        {
            using var document = JsonDocument.Parse(payload);
            var root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object ||
                !root.TryGetProperty("version", out var version) || version.ValueKind != JsonValueKind.Number || !version.TryGetInt32(out var versionNumber) || versionNumber != 1 ||
                !root.TryGetProperty("seq", out var sequenceValue) || sequenceValue.ValueKind != JsonValueKind.Number || !sequenceValue.TryGetUInt64(out var sequence) || sequence == ulong.MaxValue ||
                !root.TryGetProperty("actions", out var actions) || actions.ValueKind != JsonValueKind.Array)
                return true;
            var count = actions.GetArrayLength();
            if (count > 256 || sequence < (ulong)count) return true;
            // Validate the whole batch before accepting any part of it.
            foreach (var action in actions.EnumerateArray())
                if (action.ValueKind != JsonValueKind.String || string.IsNullOrWhiteSpace(action.GetString()) || action.GetString()!.Length > 128)
                    return true;
            var epoch = root.TryGetProperty("epoch", out var epochValue) && epochValue.ValueKind == JsonValueKind.Number && epochValue.TryGetUInt64(out var parsedEpoch) ? parsedEpoch : 0;
            if (epoch != s_uiActionEpoch)
            {
                s_uiActionEpoch = epoch;
                s_uiActionSequence = 0;
                s_uiActionAckPayload = null;
                gameModule.ResetUiActions();
            }
            var first = sequence - (ulong)count + 1;
            if (first > s_uiActionSequence + 1) return true;
            var current = first;
            foreach (var action in actions.EnumerateArray())
            {
                if (current > s_uiActionSequence)
                {
                    if (!gameModule.EnqueueUiAction(action.GetString()!)) break;
                    s_uiActionSequence = current;
                }
                current++;
            }
            if (path is not null)
            {
                var ack = JsonSerializer.Serialize(new { epoch = s_uiActionEpoch, seq = s_uiActionSequence });
                if (ack != s_uiActionAckPayload)
                {
                File.WriteAllText(path + ".ack.tmp", ack);
                File.Move(path + ".ack.tmp", path + ".ack", overwrite: true);
                s_uiActionAckPayload = ack;
                }
            }
            var acknowledged = gameModule.AcknowledgeUiActions(s_uiActionEpoch, s_uiActionSequence);
            return sequence <= s_uiActionSequence && (path is not null || acknowledged);
        }
        catch (JsonException) { return true; }
    }
}
