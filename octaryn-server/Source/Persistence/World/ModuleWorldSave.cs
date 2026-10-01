using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Time;
using Octaryn.Shared.GameModules;

namespace Octaryn.Server.Persistence.World;

internal sealed class ModuleWorldSave : IDisposable
{
    private readonly IGameModuleSaveState _module;
    private readonly PlayerController _player;
    private readonly WorldTimeClock _clock;
    private readonly GameModuleManifest _manifest;
    private readonly OrderedSaveQueue<WorldSaveRecord> _queue;
    private ulong _generation;
    private uint _sessionId;
    private double _elapsed;

    internal ModuleWorldSave(string directory, GameModuleManifest manifest, IGameModuleSaveState module,
        PlayerController player, WorldTimeClock clock)
    {
        _manifest = manifest; _module = module; _player = player; _clock = clock;
        var store = new WorldSaveStore(directory);
        if (store.Read(manifest.ModuleId, manifest.Compatibility.SaveCompatibilityId) is { } saved)
        {
            if (NativePlayerSimulation.TryCreateStateFromSave(new(saved.Player.X, saved.Player.Y, saved.Player.Z,
                saved.Player.Pitch, saved.Player.Yaw), out var restored) == false)
                throw new InvalidDataException("World save player state cannot be restored.");
            module.RestoreSaveState(saved.ModuleState);
            clock.RestoreSave(saved.Clock);
            player.SetState(restored);
            _generation = saved.Generation;
            _sessionId = saved.SessionId;
            Console.WriteLine($"server_world_save loaded=1 generation={_generation} module={manifest.ModuleId}");
        }
        else Console.WriteLine($"server_world_save loaded=0 generation=0 module={manifest.ModuleId}");
        _sessionId = checked(_sessionId + 1);
        module.BeginSaveSession(_sessionId);
        // Reserve the receipt namespace durably before any gameplay can run.
        var initial = Capture();
        store.Write(initial); _generation = initial.Generation;
        _queue = new(store.Write, "octaryn-world-save");
    }

    internal void Tick(double deltaSeconds)
    {
        if (!_queue.HasCapacity) return;
        _elapsed += Math.Max(0, deltaSeconds);
        if (_elapsed < 5) return;
        Enqueue(); _elapsed = 0;
    }

    private void Enqueue()
    {
        var record = Capture();
        if (!_queue.TryEnqueue(record)) throw new InvalidOperationException("World save admission changed on its owner thread.");
        _generation = record.Generation;
    }

    private WorldSaveRecord Capture()
    {
        var bytes = new byte[IGameModuleSaveState.MaximumBytes];
        var length = _module.CaptureSaveState(bytes);
        if (length is <= 0 or > IGameModuleSaveState.MaximumBytes)
            throw new InvalidOperationException("Module save snapshot exceeds its contract.");
        var pose = _player.Snapshot();
        var record = new WorldSaveRecord(1, _manifest.ModuleId, _manifest.Compatibility.SaveCompatibilityId,
            checked(_generation + 1), _sessionId, new(pose.X, pose.Y, pose.Z, pose.Pitch, pose.Yaw),
            _clock.CaptureSave(), bytes.AsSpan(0, length).ToArray());
        return record;
    }

    public void Dispose()
    {
        try { _queue.Flush(); Enqueue(); _queue.Flush(); }
        finally { _queue.Dispose(); }
    }
}
