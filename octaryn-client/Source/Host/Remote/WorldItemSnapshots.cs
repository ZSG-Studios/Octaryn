using System.Runtime.InteropServices;
using Octaryn.Shared.Host.Api;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 88)]
internal struct WorldItemVisualPose
{
    public HostWorldItemPose Current;
    public float PreviousX, PreviousY, PreviousZ;
    public uint Reserved;
    public ulong PreviousTick;
}

internal sealed record WorldItemSnapshot(ulong Revision, WorldItemVisualPose[] Poses);

// Mutated only by transport; presentation reads immutable completed-batch snapshots.
internal sealed class WorldItemSnapshots
{
    private readonly Dictionary<ulong, WorldItemVisualPose> _items = new();
    private WorldItemSnapshot _published = new(0, []);
    private ulong _epoch, _sequence, _revision;
    private bool _baseline;
    public WorldItemSnapshot Published => Volatile.Read(ref _published);

    public bool Accept(ReadOnlySpan<byte> bytes, out byte[]? acknowledgement)
    {
        acknowledgement = null;
        if (!WorldItemPacket.Decode(bytes, out var epoch, out var sequence, out var flags, out var poses)) return false;
        if (epoch == _epoch && sequence <= _sequence)
        {
            if ((flags & WorldItemPacket.EndBatch) != 0) acknowledgement = WorldItemPacket.Acknowledge(epoch, sequence);
            return true;
        }
        if ((flags & WorldItemPacket.BeginBaseline) != 0)
        {
            if (sequence != 1 || epoch <= _epoch) return false;
            _epoch = epoch; _sequence = 0; _baseline = true; _items.Clear();
        }
        if (epoch != _epoch || sequence != _sequence + 1) return false;
        foreach (ref readonly var pose in poses)
        {
            var exists = _items.TryGetValue(pose.EntityId, out var previous);
            if ((pose.Flags & HostWorldItemPose.Removed) != 0)
            {
                if (exists && previous.Current.Generation == pose.Generation) _items.Remove(pose.EntityId);
                continue;
            }
            if (!exists && _items.Count >= WorldItemPacket.MaximumItems) return false;
            if (exists && previous.Current.Generation > pose.Generation) continue;
            var same = exists && previous.Current.Generation == pose.Generation;
            _items[pose.EntityId] = new WorldItemVisualPose
            {
                Current = pose,
                PreviousX = same ? previous.Current.X : pose.X,
                PreviousY = same ? previous.Current.Y : pose.Y,
                PreviousZ = same ? previous.Current.Z : pose.Z,
                PreviousTick = same ? previous.Current.SourceTick : pose.SourceTick
            };
        }
        _sequence = sequence;
        if ((flags & WorldItemPacket.EndBaseline) != 0)
        {
            if (!_baseline || (flags & WorldItemPacket.EndBatch) == 0) return false;
            _baseline = false;
        }
        if ((flags & WorldItemPacket.EndBatch) != 0)
        {
            if (!_baseline)
                Volatile.Write(ref _published, new WorldItemSnapshot(++_revision, _items.Values.ToArray()));
            acknowledgement = WorldItemPacket.Acknowledge(epoch, sequence);
        }
        return true;
    }

    public void ResetConnection()
    {
        _items.Clear(); _epoch = _sequence = 0; _baseline = false;
        Volatile.Write(ref _published, new WorldItemSnapshot(++_revision, []));
    }
}
