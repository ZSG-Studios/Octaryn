using Arch.Core;
using Octaryn.Server.Persistence.World;

namespace Octaryn.Server.Simulation.Players;

// One authority world; the server clock supplies accepted commands and owns the rate.
internal sealed partial class PlayerSimulationWorld : IDisposable
{
    private readonly Arch.Core.World _world = Arch.Core.World.Create();
    private readonly Dictionary<int, (PlayerSimulationIdentity Identity, Entity Entity)> _players = new();
    private readonly NativePlayerSimulation _simulation;
    private IntPtr? _mapWorld;
    private long _generation;
    private bool _disposed;

    public PlayerSimulationWorld()
    {
        _simulation = new NativePlayerSimulation();
    }

    public PlayerSimulationIdentity Add(int id, PlayerState initial, bool loadedFromSave = false)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (_players.ContainsKey(id)) throw new InvalidOperationException("Player identity is already active.");
        var identity = new PlayerSimulationIdentity(id, checked(++_generation));
        var body = new BodyComponent(NativePlayerSimulation.CreateSession(initial, loadedFromSave));
        try
        {
            var entity = _world.Create(new IdentityComponent { Value = identity },
                new StateComponent { Value = NativePlayerSimulation.StateFromSession(body.Handle) },
                new CommandComponent(), body);
            _players.Add(id, (identity, entity));
            return identity;
        }
        catch
        {
            NativePlayerSimulation.DestroySession(body.Handle);
            throw;
        }
    }

    private Entity Find(PlayerSimulationIdentity identity)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_players.TryGetValue(identity.Id, out var entry) || entry.Identity != identity)
            throw new InvalidOperationException("Player identity is no longer active.");
        return entry.Entity;
    }

    public PlayerState Snapshot(PlayerSimulationIdentity identity) => _world.Get<StateComponent>(Find(identity)).Value;

    public bool CollisionReady(PlayerSimulationIdentity identity, double deltaSeconds = 1.0 / 60.0)
    {
        if (!_mapWorld.HasValue) return true;
        var state = Snapshot(identity);
        var dt = double.IsFinite(deltaSeconds) ? (float)Math.Clamp(deltaSeconds, 0, .25) : 0;
        // Fly input permits both horizontal axes at once without normalization.
        var speed = MathF.Max(141.422f, MathF.Max(MathF.Abs(state.VelocityX), MathF.Abs(state.VelocityZ)));
        return Octaryn.Server.World.MapWorld.NativeMapWorld.CollisionReady(_mapWorld.Value, state.X, state.Z, 2 + dt * speed);
    }

    // Module-driven authority: writes the module-computed state into both the
    // snapshot component and the native session so persistence stays coherent.
    public void SetState(PlayerSimulationIdentity identity, PlayerState state)
    {
        var entity = Find(identity);
        _world.Get<StateComponent>(entity).Value = state;
        NativePlayerSimulation.WriteSessionState(_world.Get<BodyComponent>(entity).Handle, state);
    }

    public bool LoadedFromSave(PlayerSimulationIdentity identity) =>
        NativePlayerSimulation.SessionLoadedFromSave(_world.Get<BodyComponent>(Find(identity)).Handle);

    // Map mode: the session applies the manifest spawn pose through the map world.
    public bool AlignSpawnWithMap(PlayerSimulationIdentity identity, out PlayerState spawned)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_mapWorld.HasValue) throw new InvalidOperationException("Map world is not attached.");
        var entity = Find(identity);
        var body = _world.Get<BodyComponent>(entity);
        NativePlayerSimulation.SpawnFromMap(_mapWorld.Value, body.Handle);
        spawned = NativePlayerSimulation.StateFromSession(body.Handle);
        _world.Get<StateComponent>(entity).Value = spawned;
        return true;
    }

    internal void AttachMapWorld(IntPtr mapWorld) => _mapWorld = mapWorld;

    internal bool PrepareSave(PlayerSimulationIdentity identity, double deltaSeconds, bool force,
        out NativePersistencePlayerState saved)
    {
        var entity = Find(identity);
        var state = _world.Get<StateComponent>(entity).Value;
        saved = new NativePersistencePlayerState(state.X, state.Y, state.Z, state.Pitch, state.Yaw);
        return NativePlayerSimulation.SaveDecision(_world.Get<BodyComponent>(entity).Handle,
            deltaSeconds, force).ShouldSave != 0;
    }

    internal void NoteSaved(PlayerSimulationIdentity identity, NativePersistencePlayerState saved) =>
        NativePlayerSimulation.NoteSaved(_world.Get<BodyComponent>(Find(identity)).Handle, saved);

    public void Remove(PlayerSimulationIdentity identity)
    {
        var entity = Find(identity);
        NativePlayerSimulation.DestroySession(_world.Get<BodyComponent>(entity).Handle);
        _world.Destroy(entity);
        _players.Remove(identity.Id);
    }

    public void Dispose()
    {
        if (_disposed) return;
        foreach (var entry in _players.Values)
            NativePlayerSimulation.DestroySession(_world.Get<BodyComponent>(entry.Entity).Handle);
        _players.Clear();
        Arch.Core.World.Destroy(_world);
        _disposed = true;
    }
}
