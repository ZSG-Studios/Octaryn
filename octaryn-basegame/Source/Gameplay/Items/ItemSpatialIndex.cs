using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Items;

// Module-owned broadphase for items created by ItemSystem. Exact radius and
// ray tests stay in the gameplay systems; cells only remove distant work.
internal sealed class ItemSpatialIndex
{
    private const float CellSize = 4;
    private readonly Dictionary<(int, int, int), HashSet<ulong>> _cells = new();
    private readonly Dictionary<ulong, (int, int, int)> _locations = new();
    private readonly HashSet<ulong> _awake = new();
    private readonly Stack<HashSet<ulong>> _emptyCells = new();

    private static (int, int, int) Cell(float x, float y, float z) =>
        ((int)MathF.Floor(x / CellSize), (int)MathF.Floor(y / CellSize), (int)MathF.Floor(z / CellSize));

    public void Update(ModuleEntity entity, in WorldItemComponent item)
    {
        var cell = Cell(item.X, item.Y, item.Z);
        if (!_locations.TryGetValue(entity.Id, out var previous) || previous != cell)
        {
            RemoveCell(entity.Id);
            if (!_cells.TryGetValue(cell, out var entries))
                _cells[cell] = entries = _emptyCells.Count > 0 ? _emptyCells.Pop() : new();
            entries.Add(entity.Id);
            _locations[entity.Id] = cell;
        }
        if (item.Grounded) _awake.Remove(entity.Id);
        else _awake.Add(entity.Id);
    }

    private void RemoveCell(ulong id)
    {
        if (!_locations.Remove(id, out var previous)) return;
        var entries = _cells[previous];
        entries.Remove(id);
        if (entries.Count == 0)
        {
            _cells.Remove(previous);
            if (_emptyCells.Count < 256 && entries.EnsureCapacity(0) <= 1024) _emptyCells.Push(entries);
        }
    }

    public void Remove(ModuleEntity entity)
    {
        RemoveCell(entity.Id);
        _awake.Remove(entity.Id);
    }

    public void Awake(List<ulong> result)
    {
        result.Clear();
        result.AddRange(_awake);
        result.Sort();
    }

    public void Nearby(float x, float y, float z, float radius, List<ulong> result)
    {
        result.Clear();
        var low = Cell(x - radius, y - radius, z - radius);
        var high = Cell(x + radius, y + radius, z + radius);
        for (var cx = low.Item1; cx <= high.Item1; ++cx)
        for (var cy = low.Item2; cy <= high.Item2; ++cy)
        for (var cz = low.Item3; cz <= high.Item3; ++cz)
            if (_cells.TryGetValue((cx, cy, cz), out var entries)) result.AddRange(entries);
        result.Sort();
    }
}
