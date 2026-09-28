using Octaryn.Server.Host;
using Octaryn.Shared.ApiExposure;
using Octaryn.Server.Modules.Bundled;
using Octaryn.Server.Persistence.World;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.Tick;
using Octaryn.Server.Validation;
using Octaryn.Server.World.MapWorld;
using Octaryn.Server.World.Time;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Time;

namespace Octaryn.Server.Modules;

internal sealed partial class ModuleActivator : IDisposable
{
    private readonly IGameModuleRegistration _registration;
    private readonly bool _requiresBundledMetadata;
    private readonly WorldTimeClock _worldTime = new();
    private readonly PlayerController _playerController;
    private readonly PlayerSimulationWorld _playerSimulation;
    private readonly NativeScheduleRuntime _scheduleRuntime = new();
    private readonly AuthorityTickRunner _authorityTick;
    private readonly ModuleTickProfile _moduleProfile = new();
    private readonly ModuleTickCall _moduleTickCall;
    private ModuleFrameContext _moduleFrame;
    private readonly IntPtr? _mapWorld;
    private ulong _lastTickId;
    private WorldTime _lastWorldTime;
    private IGameModuleInstance? _instance;
    private ServerHostApiProvider? _managedApis;
    private bool _isDisposed;

    // Environment-selected map mode: a GLB mesh map provides the world.
    private static class MapWorld
    {
        internal static readonly bool Enabled = ParseEnabled();
        internal static string? GlbPath => Environment.GetEnvironmentVariable("OCTARYN_SERVER_MAP_PATH");
        internal static string? ManifestPath => Environment.GetEnvironmentVariable("OCTARYN_SERVER_MAP_MANIFEST_PATH");

        private static bool ParseEnabled()
        {
            var value = Environment.GetEnvironmentVariable("OCTARYN_SERVER_MAP_MODE");
            return value is "1" or "true" or "TRUE" or "yes" or "YES";
        }
    }

    public ModuleActivator()
        : this(Loader.LoadBundledRegistration(), requiresBundledMetadata: true)
    {
    }

    public ModuleActivator(IGameModuleRegistration registration)
        : this(registration, requiresBundledMetadata: false)
    {
    }

    private ModuleActivator(IGameModuleRegistration registration, bool requiresBundledMetadata)
    {
        _registration = registration;
        _requiresBundledMetadata = requiresBundledMetadata;
        _mapWorld = MapWorld.Enabled ? CreateMapWorld() : null;

        _playerSimulation = new PlayerSimulationWorld();
        if (_mapWorld is { } mapWorld)
        {
            _playerSimulation.AttachMapWorld(mapWorld);
            var spawn = NativeMapWorld.SpawnState(mapWorld);
            LiveDebugLog.Write(
                $"server_live_map_world active=1 triangles={NativeMapWorld.TriangleCount(mapWorld)} " +
                $"spawn=({spawn.X:F3},{spawn.Y:F3},{spawn.Z:F3})");
        }
        else
        {
            LiveDebugLog.Write("server_live_map_world active=0 reason=map_mode_disabled player_simulation=unavailable");
        }
        _playerController = new PlayerController(
            NativeWorldPersistenceLibrary.PlayerDirectoryPathFromEnvironment(), _playerSimulation);
        _authorityTick = new AuthorityTickRunner(_scheduleRuntime, _playerController, _worldTime);
        _moduleTickCall = new ModuleTickCall(_scheduleRuntime, RunModuleTick);
    }

    private IntPtr CreateMapWorld()
    {
        var glbPath = MapWorld.GlbPath;
        if (string.IsNullOrWhiteSpace(glbPath))
        {
            throw new InvalidOperationException("OCTARYN_SERVER_MAP_MODE=1 requires OCTARYN_SERVER_MAP_PATH.");
        }

        var mapWorld = NativeMapWorld.Create(glbPath, MapWorld.ManifestPath);
        if (mapWorld == IntPtr.Zero)
        {
            throw new InvalidOperationException($"Native map world load failed for {glbPath}.");
        }

        return mapWorld;
    }

    public bool IsActive => _instance is not null;
    internal ulong AuthorityTickId => _lastTickId;
    internal Networking.Remote.WorldItemRegistry? WorldItems => _managedApis?.WorldItems;

    internal WorldTimeSnapshot SnapshotWorldTime()
    {
        return _worldTime.Snapshot();
    }

    internal PlayerState SnapshotPlayer()
    {
        return _playerController.Snapshot();
    }

    internal void SetWorldTimeSpeedMultiplier(double multiplier)
    {
        _worldTime.SetSpeedMultiplier(multiplier);
    }

    internal void SetWorldTimeHourOffset(int offset) => _worldTime.SetHourOffset(offset);

    // Called by RemoteServer when a dedicated session starts; module
    // replication calls report false until a channel is attached.
    internal void AttachReplicationChannel(Networking.Remote.IServerReplicationChannel channel)
    {
        _managedApis?.SetReplicationChannel(channel);
    }

    internal bool AcknowledgeUiActions(ulong epoch, ulong sequence) => _managedApis?.AcknowledgeUiActions(epoch, sequence) ?? false;

    internal void ResetUiActions() => _managedApis?.ResetUiActions();

    internal bool EnqueueUiAction(string actionId) => _managedApis?.EnqueueUiAction(actionId) ?? false;

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
            LiveDebugLog.Write("server_live_module_validation valid=0");
            return -2;
        }
        LiveDebugLog.Write("server_live_module_validation valid=1");

        var bundledManifest = Catalog.ResolveManifest(_registration.Manifest.ModuleId);
        if ((bundledManifest is null && _requiresBundledMetadata) ||
            (bundledManifest is not null && !BundledModuleMetadataVerifier.Matches(bundledManifest, _registration.Manifest)))
        {
            LiveDebugLog.Write($"server_live_bundled_module valid=0 module={_registration.Manifest.ModuleId}");
            return -3;
        }
        LiveDebugLog.Write($"server_live_bundled_module valid=1 module={_registration.Manifest.ModuleId}");

        // Map worlds are the only world simulation; without one a client join
        // would crash the first player tick. Refuse activation instead.
        if (_mapWorld is null)
        {
            LiveDebugLog.Write("server_live_map_world required=1 active=0");
            return -4;
        }

        try
        {
            var apis = commandSink is HostBridge.NativeHostBridge bridge &&
                bridge.CreateApiProvider() is { } nativeApis
                ? nativeApis
                : new ServerHostApiProvider(() => _lastTickId, _mapWorld ?? IntPtr.Zero, _scheduleRuntime, _playerController);
            _managedApis = apis as ServerHostApiProvider;
            _instance = _registration.CreateInstance(
                HostModuleContext.Create(_registration.Manifest, commandSink, apis));
            _playerController.ApplyMapSpawn();
            // Module player authority: consumed commands route through the
            // module step; the native simulation step stays the fallback.
            if (_instance is IGameModulePlayerAuthority playerAuthority &&
                (_registration.Manifest.RequestedHostApis ?? []).Contains(
                    HostApiIds.Player, StringComparer.Ordinal))
            {
                _playerController.StepOverride = frame =>
                {
                    _managedApis?.SetLatestInput(frame.Input);
                    playerAuthority.TickPlayer(new ModuleFrameContext(
                        frame.DeltaSeconds, frame.FrameIndex, _lastWorldTime));
                };
                LiveDebugLog.Write("server_live_player_authority authority=module");
            }

            LiveDebugLog.Write("server_live_activate active=1");
        }
        catch
        {
            _instance?.Dispose();
            _instance = null;
            throw;
        }

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
        _moduleProfile.Begin();
        _managedApis?.SetLatestInput(frame.Input);
        var worldTime = _authorityTick.Execute(in frame, static () => 0, out _);
        var authorityDone = _moduleProfile.Mark();
        _lastWorldTime = worldTime;
        _lastTickId = worldTime.TickId;
        _moduleFrame = new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex, worldTime);
        _moduleTickCall.Execute();
        _moduleProfile.End(frame.FrameIndex, authorityDone);

        LiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6}");
    }

    private void RunModuleTick() => _instance!.Tick(in _moduleFrame);

    internal void TickHostOnly(in HostFrameSnapshot snapshot)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        var frame = HostFrameContext.FromSnapshot(in snapshot);
        var worldTime = _authorityTick.Execute(in frame, static () => 0, out _);
        _lastWorldTime = worldTime;
        _lastTickId = worldTime.TickId;
        LiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6} host_only=1 module={_registration.Manifest.ModuleId}");
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
            _managedApis?.Dispose();
            try { _playerController.Dispose(); }
            finally
            {
                _playerSimulation.Dispose();
                if (_mapWorld is { } mapWorld)
                {
                    NativeMapWorld.Destroy(mapWorld);
                }
            }
            _authorityTick.Dispose();
            _moduleProfile.Dispose();
            _moduleTickCall.Dispose();
            _scheduleRuntime.Dispose();
            _worldTime.Dispose();
            _instance = null;
        }
    }
}
