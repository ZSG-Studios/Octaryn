using Arch.Core;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.Simulation.Players;

// One authority world; the server clock supplies accepted commands and owns the rate.
internal sealed partial class PlayerSimulationWorld : IDisposable
{
    private readonly Arch.Core.World _world = Arch.Core.World.Create();
    private readonly Dictionary<int, (PlayerSimulationIdentity Identity, Entity Entity)> _players = new();
    private readonly NativePlayerSimulation _simulation;
    private long _generation;
    private bool _disposed;

    public PlayerSimulationWorld(BlockStore blocks, IBlockAuthorityRules rules,
        Func<BlockPosition, BlockId>? generatedBlocks = null)
    {
        _simulation = new NativePlayerSimulation(blocks, rules, generatedBlocks);
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

    public bool Intersects(PlayerSimulationIdentity identity, int x, int y, int z) =>
        NativePlayerSimulation.SessionIntersectsBlock(_world.Get<BodyComponent>(Find(identity)).Handle, x, y, z);

    public bool LoadedFromSave(PlayerSimulationIdentity identity) =>
        NativePlayerSimulation.SessionLoadedFromSave(_world.Get<BodyComponent>(Find(identity)).Handle);

    public bool AlignSpawn(PlayerSimulationIdentity identity, out PlayerState aligned, out bool adjusted,
        out int surfaceY, out BlockId surfaceBlock)
    {
        var entity = Find(identity);
        var success = _simulation.TryAlignSpawnToSurface(_world.Get<BodyComponent>(entity).Handle,
            out aligned, out adjusted, out surfaceY, out surfaceBlock);
        _world.Get<StateComponent>(entity).Value = aligned;
        return success;
    }

    public bool SaveIfDue(PlayerSimulationIdentity identity, string directory, double deltaSeconds, bool force)
    {
        var entity = Find(identity);
        var body = _world.Get<BodyComponent>(entity);
        if (NativePlayerSimulation.SaveDecision(body.Handle, deltaSeconds, force).ShouldSave == 0) return false;
        var state = _world.Get<StateComponent>(entity).Value;
        var saved = new NativePersistencePlayerState(state.X, state.Y, state.Z,
            state.Pitch, state.Yaw, state.SelectedBlock.Value);
        NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(directory, identity.Id, saved);
        NativePlayerSimulation.NoteSaved(body.Handle, saved);
        return true;
    }

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
