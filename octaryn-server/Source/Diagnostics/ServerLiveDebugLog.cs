namespace Octaryn.Server;

internal static class ServerLiveDebugLog
{
    private const string LogPathEnvironmentVariable = "OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH";

    private static readonly object s_lock = new();
    private static readonly Lazy<StreamWriter?> s_log = new(OpenLog);

    public static void Write(FormattableString message)
    {
        Console.WriteLine(FormattableString.Invariant(message));
        WriteFileLine(FormattableString.Invariant(message));
    }

    public static void Write(string message)
    {
        Console.WriteLine(message);
        WriteFileLine(message);
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
