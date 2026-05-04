namespace Octaryn.Shared.Host;

internal static class HostCommandWriteScope
{
    [ThreadStatic]
    private static int depth;

    public static bool IsActive => depth > 0;

    public static IDisposable Enter(HostWorkAccess access)
    {
        if ((access & HostWorkAccess.CommandSinkWrite) == 0)
        {
            return EmptyScope.Instance;
        }

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

    private sealed class EmptyScope : IDisposable
    {
        public static readonly EmptyScope Instance = new();

        private EmptyScope()
        {
        }

        public void Dispose()
        {
        }
    }
}
