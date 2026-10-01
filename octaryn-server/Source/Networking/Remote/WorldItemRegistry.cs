using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Networking.Remote;

// Owner-thread latest poses and bounded tombstones. Transform updates coalesce; receipts do not.
internal sealed class WorldItemRegistry
{
    public const int MaximumItems = 10000;
    private readonly Dictionary<ulong, HostWorldItemPose> _items = new();
    private readonly Dictionary<ulong, HostWorldItemPose> _dirty = new();
    private readonly Queue<ulong> _order = new();
    private ulong _generation;
    private int _removed;
    public int Available => MaximumItems - _items.Count - _removed;
    public int Count => _items.Count;

    public bool Publish(in HostWorldItemPose input, ulong tick)
    {
        if (!input.IsValid) return false;
        var pose = input;
        if (_items.TryGetValue(pose.EntityId, out var previous)) pose.Generation = previous.Generation;
        else
        {
            if ((pose.Flags & HostWorldItemPose.Removed) != 0) return true;
            if (Available == 0 || _dirty.ContainsKey(pose.EntityId)) return false;
            pose.Generation = ++_generation;
        }
        pose.SourceTick = tick;
        if ((pose.Flags & HostWorldItemPose.Removed) != 0) { _items.Remove(pose.EntityId); ++_removed; }
        else _items[pose.EntityId] = pose;
        if (!_dirty.ContainsKey(pose.EntityId)) _order.Enqueue(pose.EntityId);
        _dirty[pose.EntityId] = pose;
        return true;
    }

    public HostWorldItemPose[] BeginBaseline()
    {
        _dirty.Clear(); _order.Clear(); _removed = 0;
        return _items.Values.OrderBy(pose => pose.EntityId).ToArray();
    }

    public int Drain(Span<HostWorldItemPose> output)
    {
        var count = 0;
        while (count < output.Length && _order.TryDequeue(out var id))
        {
            var pose = _dirty[id];
            _dirty.Remove(id);
            if ((pose.Flags & HostWorldItemPose.Removed) != 0) --_removed;
            output[count++] = pose;
        }
        return count;
    }
}
