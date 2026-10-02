namespace Octaryn.Shared.Host.Api;

// Vends domain APIs to modules. Hosts implement this over either the native
// query function or managed defaults; null means the API is unavailable.
public interface IHostApiProvider
{
    IHostScenePhysicsApi? GetScenePhysicsApi() => null;
    IHostResidencyApi? GetResidencyApi() => null;
    IHostGraphicsApi? GetGraphicsApi() => null;
    IHostApplicationApi? GetApplicationApi() => null;
    IHostSceneTransitionApi? GetTransitionApi(GameModules.GameModuleManifest manifest,string moduleRoot) => null;
    IHostSceneApi? GetSceneApi(GameModules.GameModuleManifest manifest, string moduleRoot) => null;

    IHostContentApi? GetContentApi(GameModules.GameModuleManifest manifest, string moduleRoot)
    {
        return new DeclaredContentApi(manifest, moduleRoot);
    }

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

    IHostUiApi? GetUiApi(GameModules.GameModuleManifest manifest, string moduleRoot)
    {
        var backend = GetUiApi();
        return backend is null ? null : new DeclaredUiApi(backend, manifest, moduleRoot);
    }

    IHostReplicationApi? GetReplicationApi();
}
