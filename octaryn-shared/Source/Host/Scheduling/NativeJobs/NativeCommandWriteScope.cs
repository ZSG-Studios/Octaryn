namespace Octaryn.Shared.Host;

internal static class NativeCommandWriteScope
{
    public static bool IsActive => NativeJobsLibrary.IsCommandWriteScopeActive;

    public static IDisposable Enter()
    {
        NativeJobsLibrary.EnterCommandWriteScope();
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
            NativeJobsLibrary.ExitCommandWriteScope();
        }
    }
}
