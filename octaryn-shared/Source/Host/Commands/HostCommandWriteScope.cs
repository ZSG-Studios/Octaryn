namespace Octaryn.Shared.Host;

internal static class HostCommandWriteScope
{
    [ThreadStatic]
    private static int depth;

    public static bool IsActive => depth > 0;

    public static IDisposable EnterCommandWrite()
    {
        depth++;
        return new ActiveScope();
    }

    private sealed class ActiveScope : IDisposable
    {
        private bool isDisposed;

        public void Dispose()
        {
            if (isDisposed)
            {
                return;
            }

            isDisposed = true;
            depth--;
        }
    }
}
