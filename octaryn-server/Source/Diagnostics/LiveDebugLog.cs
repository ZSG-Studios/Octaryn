namespace Octaryn.Server;

internal static class LiveDebugLog
{
    private const string LogPathEnvironmentVariable = "OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH";
    private const string FilterSteadyEnvironmentVariable = "OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY";

    private static readonly object s_lock = new();
    private static readonly Lazy<StreamWriter?> s_log = new(OpenLog);
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
        if (ShouldFilterSteadyMessage(message))
        {
            return;
        }
        Console.WriteLine(message);
        WriteFileLine(message);
    }

    private static bool ShouldFilterSteadyMessage(string message)
    {
        return s_filterSteady.Value && (
            message.StartsWith("server_live_tick ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_player_state ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_player_input_intent active=1 ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_world_time_intent active=1 ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_chunk_view_intent ", StringComparison.Ordinal) ||
            message.StartsWith("server_live_chunk_stream active=1 skipped=1 ", StringComparison.Ordinal) ||
            message == "server_live_client_command_drain applied=0 pending=0");
    }

    private static void WriteFileLine(string message)
    {
        var log = s_log.Value;
        if (log is null)
        {
            return;
        }

        lock (s_lock)
        {
            log.WriteLine(message);
            log.Flush();
        }
    }

    private static StreamWriter? OpenLog()
    {
        var path = Environment.GetEnvironmentVariable(LogPathEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(path))
        {
            return null;
        }

        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        return new StreamWriter(path, append: false)
        {
            AutoFlush = true
        };
    }
}
