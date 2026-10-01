using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class SchedulingApi : IHostSchedulingApi
    {
        private readonly NativeScheduleRuntime _runtime;

        public SchedulingApi(NativeScheduleRuntime runtime)
        {
            _runtime = runtime;
        }

        public void RunOnMainThread(string jobId, Action work)
        {
            _runtime.ExecuteCommandWriteMainThread(jobId, work);
        }

        public void RunOnWorker(string jobId, Action work)
        {
            _runtime.ExecuteWorker(jobId, work);
        }
    }
}
