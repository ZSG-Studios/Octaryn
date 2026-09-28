using Octaryn.Client.Validation;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Client.Host;

internal sealed class GameModuleActivator : IDisposable
{
    private readonly IGameModuleRegistration _registration;
    private readonly bool _requiresBundledMetadata;
    private readonly NativeScheduleRuntime _scheduleRuntime = new();
    private IGameModuleInstance? _instance;
    private ClientHostApiProvider? _managedApis;
    private bool _isDisposed;

    public GameModuleActivator()
        : this(BundledModuleLoader.LoadBundledRegistration(), requiresBundledMetadata: true)
    {
    }

    public GameModuleActivator(IGameModuleRegistration registration)
        : this(registration, requiresBundledMetadata: false)
    {
    }

    private GameModuleActivator(IGameModuleRegistration registration, bool requiresBundledMetadata)
    {
        _registration = registration;
        _requiresBundledMetadata = requiresBundledMetadata;
    }

    public bool IsActive => _instance is not null;

    public int Activate(IHostCommandSink commandSink, IHostApiProvider? apis = null)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);

        if (_instance is not null)
        {
            return 0;
        }

        var validationReport = ModuleValidation.Validate(_registration);
        if (!validationReport.IsValid)
        {
            return -2;
        }

        var bundledManifest = BundledModuleCatalog.ResolveManifest(_registration.Manifest.ModuleId);
        if ((bundledManifest is null && _requiresBundledMetadata) ||
            (bundledManifest is not null && !BundledModuleMetadataVerifier.Matches(bundledManifest, _registration.Manifest)))
        {
            return -3;
        }

        apis ??= new ClientHostApiProvider(_scheduleRuntime);
        _managedApis = apis as ClientHostApiProvider;
        _instance = _registration.CreateInstance(HostModuleContext.Create(_registration.Manifest, commandSink, apis));
        return 0;
    }

    public void Tick(in HostFrameSnapshot snapshot)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (_instance is null)
        {
            return;
        }

        var frame = HostFrameContext.FromSnapshot(in snapshot);
        _managedApis?.SetLatestInput(frame.Input);
        var moduleFrame = new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex);
        _scheduleRuntime.ExecuteCommandWriteMainThread(
            "client.module.tick",
            () => _instance.Tick(in moduleFrame));
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        try
        {
            _instance?.Dispose();
        }
        finally
        {
            _scheduleRuntime.Dispose();
            _instance = null;
        }
    }
}
