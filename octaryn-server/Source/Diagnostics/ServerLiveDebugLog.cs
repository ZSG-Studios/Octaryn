namespace Octaryn.Server;

internal static class ServerLiveDebugLog
{
    public static void Write(FormattableString message)
    {
        Console.WriteLine(FormattableString.Invariant(message));
    }

    public static void Write(string message)
    {
        Console.WriteLine(message);
    }
}
