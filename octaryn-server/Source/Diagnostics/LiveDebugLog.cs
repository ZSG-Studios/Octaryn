using System.Diagnostics;

namespace Octaryn.Server;

internal static class LiveDebugLog
{
    private const string LogPathEnvironmentVariable = "OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH";
    private const string FilterSteadyEnvironmentVariable = "OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY";

    private static readonly object s_lock = new();
    private static long s_lastSummary;
    private static string? s_lastChunkWindow;
    private static readonly Lazy<BoundedLogWriter> s_log = new(() =>
        new BoundedLogWriter(Environment.GetEnvironmentVariable(LogPathEnvironmentVariable), console: true));
    private static readonly Lazy<bool> s_filterSteady = new(() =>
    {
        var value = Environment.GetEnvironmentVariable(FilterSteadyEnvironmentVariable);
        return value is "1" or "true" or "TRUE" or "yes" or "YES";
    });

    public static void Write(FormattableString message)
    {
        Write(FormattableString.Invariant(message));
    }

    public static void Write(string message)
    {
        lock (s_lock)
        {
            if (s_filterSteady.Value)
            {
                if (message.StartsWith("server_live_tick ", StringComparison.Ordinal))
                {
                    var now = Stopwatch.GetTimestamp();
                    if (s_lastSummary != 0 && Stopwatch.GetElapsedTime(s_lastSummary, now).TotalSeconds < 5)
                        return;
                    s_lastSummary = now;
                    message = "server_status " + message["server_live_tick ".Length..];
                }
                else if (ShouldFilterSteadyMessage(message)) return;
            }
            s_log.Value.TryWrite(message);
        }
    }

    private static bool ShouldFilterSteadyMessage(string message)
    {
        if (message.StartsWith("server_live_chunk_window ", StringComparison.Ordinal))
        {
            if (message == s_lastChunkWindow) return true;
            s_lastChunkWindow = message;
            return false;
        }
        return (
            message.StartsWith("server_live_player_state ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_player_tick_timing ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_player_input_intent active=1 ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_player_motion_profile ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_world_time_intent active=1 ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_chunk_view_intent ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_chunk_stream active=1 ", StringComparison.Ordinal) ||
            message.StartsWith("server_remote_chunk_snapshot sent=1 ", StringComparison.Ordinal) ||
            message == "server_live_client_command_drain applied=0 pending=0");
    }

    public static void Shutdown() { if (s_log.IsValueCreated) s_log.Value.Dispose(); }
}
