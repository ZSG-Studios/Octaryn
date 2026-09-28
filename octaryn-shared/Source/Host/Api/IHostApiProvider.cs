namespace Octaryn.Shared.Host.Api;

// Vends domain APIs to modules. Hosts implement this over either the native
// query function or managed defaults; null means the API is unavailable.
public interface IHostApiProvider
{
    IHostTimeApi? GetTimeApi();

    IHostDiagnosticsApi? GetDiagnosticsApi();

    IHostPhysicsApi? GetPhysicsApi();

    IHostWorldApi? GetWorldApi();

    IHostPlayerApi? GetPlayerApi();

    IHostEcsApi? GetEcsApi();

    IHostInputApi? GetInputApi();

    IHostSchedulingApi? GetSchedulingApi();

    IHostAudioApi? GetAudioApi();

    IHostUiApi? GetUiApi();

    IHostReplicationApi? GetReplicationApi();
}
