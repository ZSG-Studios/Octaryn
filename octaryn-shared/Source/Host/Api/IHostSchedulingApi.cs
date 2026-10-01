namespace Octaryn.Shared.Host.Api;

public interface IHostSchedulingApi
{
    void RunOnMainThread(string jobId, Action work);

    void RunOnWorker(string jobId, Action work);
}
