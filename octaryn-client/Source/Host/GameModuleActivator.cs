using Octaryn.Client.Validation;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;

namespace Octaryn.Client.Host;

internal sealed class GameModuleActivator : IDisposable
{
    private readonly IGameModuleRegistration _registration;
    private readonly bool _requiresBundledMetadata;
    private HostScheduler? _scheduler;
    private IGameModuleInstance? _instance;
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

    public int Activate(IHostCommandSink commandSink)
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

        var scheduler = new HostScheduler(_registration.Manifest.Schedule.Systems);
        try
        {
            _instance = _registration.CreateInstance(HostModuleContext.Create(_registration.Manifest, commandSink));
            _scheduler = scheduler;
        }
        catch
        {
            scheduler.Dispose();
            throw;
        }

        return 0;
    }

    public void Tick(in HostFrameSnapshot snapshot)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (_instance is null || _scheduler is null)
        {
            return;
        }

        var frame = HostFrameContext.FromSnapshot(in snapshot);
        var moduleFrame = new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex);
        var declaration = _registration.Manifest.Schedule.Systems[0];
        var work = HostScheduledWork.FromDeclaration(
            declaration,
            _ => _instance.Tick(in moduleFrame));
        if (!_scheduler.TryRun(work, frame))
        {
            throw new InvalidOperationException("Client module tick could not be scheduled by the host.");
        }
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
            _scheduler?.Dispose();
            _instance = null;
            _scheduler = null;
        }
    }
}
