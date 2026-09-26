using Octaryn.Server.Host;
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
    private readonly IntPtr? _mapWorld;
    private ulong _lastTickId;
    private IGameModuleInstance? _instance;
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
                : new ServerHostApiProvider(() => _lastTickId);
            _instance = _registration.CreateInstance(
                HostModuleContext.Create(_registration.Manifest, commandSink, apis));
            _playerController.ApplyMapSpawn();
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
        var worldTime = _authorityTick.Execute(in frame, static () => 0, out _);
        _lastTickId = worldTime.TickId;
        var moduleFrame = new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex, worldTime);
        _scheduleRuntime.ExecuteCommandWriteMainThread(
            "server.module.tick",
            () => _instance.Tick(in moduleFrame));

        LiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6}");
    }

    internal void TickHostOnly(in HostFrameSnapshot snapshot)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        var frame = HostFrameContext.FromSnapshot(in snapshot);
        var worldTime = _authorityTick.Execute(in frame, static () => 0, out _);
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
            try { _playerController.Dispose(); }
            finally
            {
                _playerSimulation.Dispose();
                if (_mapWorld is { } mapWorld)
                {
                    NativeMapWorld.Destroy(mapWorld);
                }
            }
            _scheduleRuntime.Dispose();
            _worldTime.Dispose();
            _instance = null;
        }
    }
}
