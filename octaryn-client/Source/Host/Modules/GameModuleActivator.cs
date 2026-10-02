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
    private IHostSceneApi? _sceneApi;
    private IDisposable? _uiApiLifetime;
    private IDisposable? _graphicsApiLifetime;
    private IDisposable? _applicationApiLifetime;
    private IDisposable? _transitionApiLifetime;
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
            foreach(var issue in validationReport.Issues)
                Console.Error.WriteLine($"client_module_validation_issue severity={issue.Severity} code={issue.Code} message={issue.Message}");
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
        var context = HostModuleContext.Create(_registration.Manifest, commandSink, apis,
            GameModuleBundle.ResolveRoot(AppContext.BaseDirectory), _scheduleRuntime);
        _sceneApi = context.Scene;
        _uiApiLifetime = context.Ui as IDisposable;
        _graphicsApiLifetime = context.Graphics as IDisposable;
        _applicationApiLifetime = context.Application as IDisposable;
        _transitionApiLifetime = context.Transition as IDisposable;
        try { _instance = _registration.CreateInstance(context); }
        catch { DisposeModuleApis(); throw; }
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
            try { DisposeModuleApis(); }
            finally { _scheduleRuntime.Dispose(); _instance = null; }
        }
    }

    private void DisposeModuleApis()
    {
        var ui = _uiApiLifetime; var scene = _sceneApi;
        _graphicsApiLifetime?.Dispose(); _graphicsApiLifetime = null;
        _applicationApiLifetime?.Dispose(); _applicationApiLifetime = null;
        _transitionApiLifetime?.Dispose(); _transitionApiLifetime = null;
        _uiApiLifetime = null; _sceneApi = null;
        try { ui?.Dispose(); }
        finally { scene?.Dispose(); }
    }
}
